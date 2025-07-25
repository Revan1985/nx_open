// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_layout_tool.h"

#include <QtCore/QBuffer>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

#include <camera/client_video_camera.h>
#include <client/client_globals.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/build_info.h>
#include <nx/core/watermark/watermark.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/export/data/nov_metadata.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <nx/vms/client/desktop/utils/timezone_helper.h>
#include <nx_ec/data/api_conversion_functions.h>

#if defined(Q_OS_WIN)
    #include <launcher/nov_launcher_win.h>
#endif

#include "../data/nov_metadata.h"

using QIODevicePtr = QScopedPointer<QIODevice>;

namespace {

const int kFileOperationRetries = 3;
const int kFileOperationRetryDelayMs = 500;

QString fileNameForResource(const QnResourcePtr& resource)
{
    QString uniqId;
    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        uniqId = camera->getPhysicalId();
    else
        uniqId = resource->getUrl();
    uniqId = QFileInfo(uniqId.mid(uniqId.indexOf('?') + 1)).completeBaseName(); // simplify name if export from existing layout
    return uniqId;
}

} // namespace

namespace nx::vms::client::desktop {

struct ExportLayoutTool::Private
{
    ExportLayoutTool* const q;
    const ExportLayoutSettings settings;
    ExportProcessError lastError = ExportProcessError::noError;
    ExportProcessStatus status = ExportProcessStatus::initial;

    // Queue of resources to export.
    QQueue<QnMediaResourcePtr> resources;

    // File that is actually written. May differ from target to avoid antivirus exe checks.
    QString actualFilename;

    // External file storage.
    QnStorageResourcePtr storage;

    // Whether any archive has been exported.
    bool exportedAnyData = false;

    /** Source layout. */
    core::LayoutResourcePtr originalLayout;

    /** Copy of the provided layout. */
    core::LayoutResourcePtr layout;

    explicit Private(ExportLayoutTool* owner, const ExportLayoutSettings& settings):
        q(owner),
        settings(settings),
        actualFilename(settings.fileName.completeFileName())
    {
    }

    void cancelExport()
    {
        // Double cancel is ok.
        if (status == ExportProcessStatus::cancelling)
            return;

        NX_ASSERT(status == ExportProcessStatus::exporting);
        setStatus(ExportProcessStatus::cancelling);
        resources.clear();

    }

    void setStatus(ExportProcessStatus value)
    {
        NX_ASSERT(status != value);
        status = value;
        emit q->statusChanged(status);
    }

    void finishExport(bool success)
    {
        NX_VERBOSE(this, "Export finished. Success: %1. Status: %2, last error: %3",
            success, (int)status, (int)lastError);

        switch (status)
        {
            case ExportProcessStatus::exporting:
                setStatus(lastError == ExportProcessError::noError
                    ? ExportProcessStatus::success
                    : ExportProcessStatus::failure);
                break;
            case ExportProcessStatus::cancelling:
                setStatus(ExportProcessStatus::cancelled);
                break;
            default:
                NX_ASSERT(false, "Should never get here. Status: %1, last error: %2",
                    (int)status, (int)lastError);
                return; //< Do nothing.
        }
        if (status != ExportProcessStatus::success)
        {
            NX_VERBOSE(this,
                "Export finished with error, temp file is removed. Status: %1, last error: %2",
                (int)status, (int)lastError);

            QFile::remove(actualFilename);
        }
        else
        {
            const auto targetFilename = settings.fileName.completeFileName();
            if (actualFilename != targetFilename)
                storage->renameFile(storage->getUrl(), targetFilename);
        }

        emit q->finished();
    }
};

ExportLayoutTool::ExportLayoutTool(
    ExportLayoutSettings settings,
    core::LayoutResourcePtr layout,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, settings))
{
    d->originalLayout = layout;
    // Make d->layout a deep exact copy of original layout.
    d->layout = (settings.mode == ExportLayoutSettings::Mode::LocalSave
        ? core::LayoutResourcePtr(new QnFileLayoutResource({}))
        : core::LayoutResourcePtr(new core::LayoutResource()));
    d->layout->setIdUnsafe(layout->getId()); //before update() uuid's must be the same
    d->layout->update(layout);

    // If exporting layout, create new guid. If layout just renamed or saved, keep guid.
    if (d->settings.mode != ExportLayoutSettings::Mode::LocalSave)
        d->layout->setIdUnsafe(nx::Uuid::createUuid());

    m_isExportToExe = nx::build_info::isWindows()
        && FileExtensionUtils::isExecutable(d->settings.fileName.extension);
}

ExportLayoutTool::~ExportLayoutTool()
{
}

bool ExportLayoutTool::prepareStorage()
{
    // Save to tmp file, then rename.
    d->actualFilename += lit(".tmp");

#ifdef Q_OS_WIN
    if (m_isExportToExe)
    {
        // TODO: #sivanov handle other errors
        if (QnNovLauncher::createLaunchingFile(d->actualFilename) != QnNovLauncher::ErrorCode::Ok)
            return false;
    }
    else
#endif
    {
        if (QFile::remove(d->actualFilename))
            NX_VERBOSE(this, "Temp file is removed");
    }

    auto fileStorage = new QnLayoutFileStorageResource(d->actualFilename);

    if (d->settings.encryption.on)
        fileStorage->setPasswordToWrite(d->settings.encryption.password);
    d->storage = QnStorageResourcePtr(fileStorage);
    return true;
}

NovMetadata ExportLayoutTool::prepareLayoutAndMetadata()
{
    NovMetadata metadata{.version = NovMetadata::kCurrentVersion};

    QSet<nx::Uuid> idList;
    common::LayoutItemDataMap items;

    for (const auto& item: d->layout->getItems())
    {
        const auto resource = core::getResourceByDescriptor(item.resource);
        const auto mediaResource = resource.dynamicCast<QnMediaResource>();
        // We only export video files or cameras; no still images, web pages etc.
        const bool skip = !mediaResource || resource->hasFlags(Qn::still_image);

        if (skip)
            continue;

        const auto id = resource->getId();
        // Only export every resource once, even if it placed on layout several times.
        if (!idList.contains(id))
        {
            d->resources << mediaResource;
            idList << id;
        }

        common::LayoutItemData localItem = item;
        localItem.resource.id = resource->getId();
        localItem.resource.path = fileNameForResource(resource);
        items.insert(localItem.uuid, localItem);

        const auto accessController = SystemContext::fromResource(resource)->accessController();
        const bool exportAllowed =
            accessController->hasPermissions(resource, Qn::ExportPermission);

        const QTimeZone tz = timeZone(mediaResource);
        metadata.itemProperties[localItem.uuid] = {
            .name = resource->getName(),
            .timeZoneOffset = resource->hasFlags(Qn::utc)
                ? tz.offsetFromUtc(
                    QDateTime::fromMSecsSinceEpoch(d->settings.period.startTimeMs, tz)) * 1000LL
                : 0,
            .timeZoneId = tz.id(),
            .flags = exportAllowed
                ? NovItemProperties::Flag::empty : NovItemProperties::Flag::noExportPermission};
    }
    d->layout->setItems(items);

    // Set up layout watermark, but only if it misses one.
    if (d->layout->data(Qn::LayoutWatermarkRole).isNull() && d->settings.watermark.visible())
        d->layout->setData(Qn::LayoutWatermarkRole, QVariant::fromValue(d->settings.watermark));

    return metadata;
}

bool ExportLayoutTool::exportMetadata(const NovMetadata& metadata)
{
    // NOV-file metadata.
    {
        QByteArray rawData = QByteArray::fromStdString(nx::reflect::json::serialize(metadata));
        if (!writeData("metadata.json", rawData))
            return false;
    }

    // Names of exported resources. Needed for compatibility with older VMS versions.
    {
        QByteArray names;
        QTextStream itemNames(&names, QIODevice::WriteOnly);
        // It is important to use the same order as d->layout->getItems() has.
        for (const auto& [itemId, _]: d->layout->getItems().asKeyValueRange())
            itemNames << metadata.itemProperties.at(itemId).name << "\n";
        itemNames.flush();

        if (!writeData("item_names.txt", names))
            return false;
    }

    // Timezones of exported resources. Needed for compatibility with older VMS versions.
    {
        QByteArray timezones;
        QTextStream itemTimeZonesStream(&timezones, QIODevice::WriteOnly);
        // It is important to use the same order as d->layout->getItems() has.
        for (const auto& [itemId, _]: d->layout->getItems().asKeyValueRange())
            itemTimeZonesStream << metadata.itemProperties.at(itemId).timeZoneOffset << "\n";
        itemTimeZonesStream.flush();

        if (!writeData("item_timezones.txt", timezones))
            return false;
    }

    // Layout items.
    {
        QByteArray layoutData;
        nx::vms::api::LayoutData layoutObject;
        ec2::fromResourceToApi(d->layout, layoutObject);
        QJson::serialize(layoutObject, &layoutData);
        /* Old name for compatibility issues. */
        if (!writeData("layout.pb", layoutData))
            return false;
    }

    // Exported period.
    {
        NX_VERBOSE(this, "Exported layout range: %1", d->settings.period);
        if (!writeData("range.bin", d->settings.period.serialize()))
            return false;
    }

    // Additional flags.
    {
        quint32 flags = d->settings.readOnly ? QnLayoutFileStorageResource::ReadOnly : 0;
        for (const QnMediaResourcePtr& resource : d->resources)
        {
            if (resource->hasFlags(Qn::utc))
            {
                flags |= QnLayoutFileStorageResource::ContainsCameras;
                break;
            }
        }

        if (!tryInLoop([this, flags]
        {
            QIODevicePtr miscFile(d->storage->open("misc.bin", QIODevice::WriteOnly));
            if (!miscFile)
                return false;
            miscFile->write((const char*)&flags, sizeof(flags));
            return true;
        }))
            return false;
    }

    // Layout id.
    {
        if (!writeData("uuid.bin", d->layout->getId().toByteArray(QUuid::WithBraces)))
            return false;
    }

    // Chunks.
    for (const auto& resource: d->resources)
    {
        QByteArray data;
        if (d->settings.bookmarks.empty())
        {
            auto systemContext = SystemContext::fromResource(resource);
            if (!NX_ASSERT(systemContext))
                continue;

            auto loader = systemContext->cameraDataManager()->loader(resource);
            if (!loader)
                continue;

            auto periods = loader->periods(Qn::RecordingContent).intersected(d->settings.period);

            NX_VERBOSE(this, "Exported recording periods for %1: %2", resource,
                nx::reflect::json::serialize(periods));
            periods.encode(data);
        }
        else
        {
            // We may not have loaded chunks when using Bookmarks export for camera, which was not
            // opened yet. This is no important as bookmarks are automatically deleted when archive
            // is not available for the given period.
            QnTimePeriodList periods;
            for (const auto& bookmark: d->settings.bookmarks)
            {
                if (bookmark.cameraId == resource->getId())
                    periods.includeTimePeriod({bookmark.startTimeMs, bookmark.durationMs});
            }
            NX_ASSERT(std::is_sorted(periods.cbegin(), periods.cend()));
            periods.encode(data);
        }

        auto fileName = nx::format("chunk_%1.bin", fileNameForResource(resource));
        if (!writeData(fileName, data))
            return false;
    }

    // Layout background.
    if (!d->layout->backgroundImageFilename().isEmpty())
    {
        auto systemContext = SystemContext::fromResource(d->originalLayout);
        if (!NX_ASSERT(systemContext))
            systemContext = appContext()->currentSystemContext();

        bool exportedLayout = d->layout->isFile();  // we have changed background to an exported layout
        ServerImageCache* cache = exportedLayout
            ? systemContext->localFileCache()
            : systemContext->serverImageCache();

        QImage background(cache->getFullPath(d->layout->backgroundImageFilename()));
        if (!background.isNull())
        {

            if (!tryInLoop([this, background]
            {
                QIODevicePtr imageFile(d->storage->open(d->layout->backgroundImageFilename(),
                    QIODevice::WriteOnly));
                if (!imageFile)
                    return false;
                background.save(imageFile.data(), "png");
                return true;
            }))
            {
                return false;
            }

            systemContext->localFileCache()->storeImageData(
                d->layout->backgroundImageFilename(),
                background);
        }
    }

    // Watermark.
    const auto watermarkData = d->layout->data(Qn::LayoutWatermarkRole);
    if (watermarkData.isValid())
    {
        const auto watermark = watermarkData.value<nx::core::Watermark>();
        if (!writeData("watermark.txt", QJson::serialized(watermark)))
            return false;
    }

    return true;
}

bool ExportLayoutTool::start()
{
    d->exportedAnyData = false;

    if (!prepareStorage())
    {
        d->lastError = ExportProcessError::fileAccess;
        d->setStatus(ExportProcessStatus::failure);
        return false;
    }

    const NovMetadata metadata = prepareLayoutAndMetadata();

    if (!exportMetadata(metadata))
    {
        d->lastError = ExportProcessError::fileAccess;
        d->setStatus(ExportProcessStatus::failure);
        return false;
    }

    emit rangeChanged(0, d->resources.size() * 100);
    emit valueChanged(0);
    d->setStatus(ExportProcessStatus::exporting);
    return exportNextCamera();
}

void ExportLayoutTool::stop()
{
    d->cancelExport();

    if (m_currentCamera)
        m_currentCamera->stopExport();

    d->finishExport(false);
}

ExportLayoutSettings::Mode ExportLayoutTool::mode() const
{
    return d->settings.mode;
}

ExportProcessError ExportLayoutTool::lastError() const
{
    return d->lastError;
}

ExportProcessStatus ExportLayoutTool::processStatus() const
{
    return d->status;
}

bool ExportLayoutTool::exportNextCamera()
{
    if (d->status != ExportProcessStatus::exporting)
        return false;

    if (d->resources.isEmpty())
    {
        if (d->exportedAnyData)
        {
            d->finishExport(true);
        }
        else
        {
            NX_VERBOSE(this, "Data not found");

            d->lastError = ExportProcessError::dataNotFound;
            d->finishExport(false);
        }
        return false;
    }

    m_offset++;
    return exportMediaResource(d->resources.dequeue());
}

bool ExportLayoutTool::exportMediaResource(const QnMediaResourcePtr& resource)
{
    m_currentCamera.reset(new QnClientVideoCamera(resource));
    connect(m_currentCamera.get(), SIGNAL(exportProgress(int)), this, SLOT(at_camera_progressChanged(int)));
    connect(m_currentCamera.get(), &QnClientVideoCamera::exportFinished, this,
        &ExportLayoutTool::at_camera_exportFinished);

    int numberOfChannels = resource->getVideoLayout()->channelCount();
    for (int i = 0; i < numberOfChannels; ++i)
    {
        QSharedPointer<QBuffer> motionFileBuffer(new QBuffer());
        motionFileBuffer->open(QIODevice::ReadWrite);
        m_currentCamera->setMotionIODevice(motionFileBuffer, i);
    }

    QString uniqId = fileNameForResource(resource);

    QnTimePeriodList playbackMask;
    for (const auto& bookmark: d->settings.bookmarks)
    {
        if (bookmark.cameraId == resource->getId())
            playbackMask.includeTimePeriod({bookmark.startTimeMs, bookmark.durationMs});
    }

    m_currentCamera->exportMediaPeriodToFile(d->settings.period,
        uniqId,
        "mkv",
        d->storage,
        timeZone(resource),
        d->settings.transcodingSettings,
        playbackMask);

    return true;
}

void ExportLayoutTool::at_camera_exportFinished(const std::optional<nx::recording::Error>& status,
    const QString& filename)
{
    if (d->status == ExportProcessStatus::cancelled)
        return;

    if (!NX_ASSERT(
        d->status != ExportProcessStatus::success && d->status != ExportProcessStatus::failure,
        "ExportLayoutTool status: %1, camera export error: %2",
        (int)d->status, (int)convertError(status)))
    {
        return;
    }

    d->lastError = convertError(status);
    if (d->lastError == ExportProcessError::dataNotFound)
    {
        NX_VERBOSE(this, "Data not found: %1", filename);

        d->lastError = ExportProcessError::noError;
        exportNextCamera();
        return;
    }

    d->exportedAnyData = true;

    if (d->lastError != ExportProcessError::noError)
    {
        NX_VERBOSE(this, "An error occurred: %1", filename);

        d->finishExport(false);
        return;
    }

    auto camera = dynamic_cast<QnClientVideoCamera*>(sender());
    if (!camera)
        return;

    int numberOfChannels = camera->resource()->getVideoLayout()->channelCount();
    bool error = false;

    for (int i = 0; i < numberOfChannels; ++i)
    {
        if (QSharedPointer<QBuffer> motionFileBuffer = camera->motionIODevice(i))
        {
            motionFileBuffer->close();
            if (error)
                continue;

            QString uniqId = fileNameForResource(camera->resource());
            QString motionFileName = lit("motion%1_%2.bin").arg(i).arg(uniqId);
            /* Other buffers must be closed even in case of error. */
            writeData(motionFileName, motionFileBuffer->buffer());
        }
    }

    if (error)
    {
        d->lastError = ExportProcessError::unsupportedMedia;
        d->finishExport(false);
    }
    else
    {
        exportNextCamera();
    }
}

void ExportLayoutTool::at_camera_progressChanged(int progress)
{
    emit valueChanged(m_offset * 100 + progress);
}

bool ExportLayoutTool::tryInLoop(std::function<bool()> handler)
{
    if (!handler)
        return false;

    QPointer<QObject> guard(this);
    int times = kFileOperationRetries;

    while (times > 0)
    {
        if (!guard)
            return false;

        if (handler())
            return true;

        times--;
        if (times <= 0)
            return false;

        QScopedPointer<QTimer> timer(new QTimer());
        timer->setSingleShot(true);
        timer->setInterval(kFileOperationRetryDelayMs);

        QScopedPointer<QEventLoop> loop(new QEventLoop());
        connect(timer.data(), &QTimer::timeout, loop.data(), &QEventLoop::quit);
        timer->start();
        loop->exec();
    }

    return false;
}

bool ExportLayoutTool::writeData(const QString &fileName, const QByteArray &data)
{
    return tryInLoop(
        [this, fileName, data]
        {
            QIODevicePtr file(d->storage->open(fileName, QIODevice::WriteOnly));
            if (!file)
                return false;
            file->write(data);
            return true;
        });
}

} // namespace nx::vms::client::desktop
