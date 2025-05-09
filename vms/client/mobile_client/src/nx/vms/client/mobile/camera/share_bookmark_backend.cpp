// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "share_bookmark_backend.h"

#include <QtQml/QtQml>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/branding.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/system_context_accessor.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/ui/share_link_helper.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/text/human_readable.h>
#include <utils/common/synctime.h>

#include "details/bookmark_constants.h"

namespace nx::vms::client::mobile {

using namespace std::chrono;

struct ShareBookmarkBackend::Private: public SystemContextAccessor
{
    ShareBookmarkBackend* const q;
    QPersistentModelIndex modelIndex;

    bool isAvailable = false;
    QnCameraBookmarksManager::BookmarkOperation bookmarkOperation;
    common::CameraBookmark bookmark;

    Private(ShareBookmarkBackend* q);

    void handleIndexUpdated();
    void shareBookmarkLink(const QString& bookmarkId) const;
    bool updateShareParams(const common::BookmarkShareableParams& shareParams);
};

ShareBookmarkBackend::Private::Private(ShareBookmarkBackend* q):
    q(q)
{
}

void ShareBookmarkBackend::Private::handleIndexUpdated()
{
    bookmark = {};

    updateFromModelIndex(modelIndex);

    if (!modelIndex.isValid())
        return;

    const auto context = systemContext();
    if (!NX_ASSERT(context))
        return;

    if (modelIndex.data(core::CameraBookmarkRole).isValid())
    {
        // Update bookmark from bookmarks model index.
        bookmarkOperation = QnCameraBookmarksManager::BookmarkOperation::update;
        bookmark = modelIndex.data(core::CameraBookmarkRole).value<common::CameraBookmark>();
        emit q->bookmarkChanged();
        return;
    }

    // Construct bookmark from analytics object model index.

    bookmarkOperation = QnCameraBookmarksManager::BookmarkOperation::create;

    const auto camera = modelIndex.data(core::ResourceRole)
        .value<QnResourcePtr>().dynamicCast<QnMediaResource>();
    const auto timestampMs = modelIndex.data(core::TimestampMsRole).value<qint64>();
    bookmark.guid = nx::Uuid::createUuid();
    bookmark.tags = { detail::BookmarkConstants::objectBasedTagName() };
    bookmark.cameraId = camera->getId();
    bookmark.creatorId = context->userWatcher()->user()->getId();
    bookmark.name = modelIndex.data(Qt::DisplayRole).toString();
    bookmark.creationTimeStampMs = qnSyncTime->value();
    bookmark.startTimeMs = milliseconds(timestampMs);
    static const qint64 kMimDurationMs = 8000;
    bookmark.durationMs = milliseconds(
        std::max<qint64>(modelIndex.data(core::DurationMsRole).value<qint64>(), kMimDurationMs));
    bookmark.description =
        [this, camera, timestampMs]()
        {
            static const QString kTemplate("%1: %2");
            const auto attributes = modelIndex.data(core::AnalyticsAttributesRole)
                .value<core::analytics::AttributeList>();

            QStringList result;

            const auto dateTime = QDateTime::fromMSecsSinceEpoch(
                timestampMs, core::ServerTimeWatcher::timeZone(camera));
            result.append(dateTime.toString());
            result.append(kTemplate.arg(tr("Camera"), camera->getName()));

            for (const auto& attribute: attributes)
            {
                result.append(kTemplate.arg(
                    attribute.displayedName, attribute.displayedValues.join(", ")));
            }
            return result.join("\n");

        }();

    emit q->bookmarkChanged();
}

void ShareBookmarkBackend::Private::shareBookmarkLink(const QString& bookmarkFullId) const
{
    const auto context = systemContext();
    if (!NX_ASSERT(context))
        return;

    const auto systemId = context->cloudSystemId();
    const auto customCloudHost = qnSettings->customCloudHost();
    const auto cloudHost = customCloudHost.isEmpty()
        ? nx::branding::cloudHost()
        : customCloudHost;

    const auto result = network::url::Builder()
        .setScheme(network::http::kSecureUrlSchemeName)
        .setHost(cloudHost)
        .setPath(QString("share/%1/%2").arg(systemId, bookmarkFullId))
        .toUrl().toQUrl();

    NX_DEBUG(this, "Shared bookmark link: ", result.toString());
    shareLink(result);
}

bool ShareBookmarkBackend::Private::updateShareParams(
    const common::BookmarkShareableParams& shareParams)
{
    const auto context = systemContext();
    if (!NX_ASSERT(context))
        return false;

    const auto manager = context->cameraBookmarksManager();
    if (!NX_ASSERT(manager))
        return false;

    bool result = false;
    const auto rollbackIfError =
        [this, &result, backupShareParams = bookmark.share]()
        {
            if (result)
                return false;

            bookmark.share = backupShareParams;
            return true;
        };

    QString fullId;
    bookmark.share = shareParams;

    QEventLoop loop;
    result = manager->changeBookmarkRest(bookmarkOperation, bookmark, nx::utils::guarded(q,
        [this, &result, &loop, &fullId](bool success, const api::BookmarkV3& updatedBookmark)
        {
            if (success)
            {
                bookmark = common::bookmarkFromApi(updatedBookmark);
                fullId = updatedBookmark.id;
            }

            result = success;
            loop.quit();
        }));

    if (rollbackIfError())
        return false;

    loop.exec();

    if (rollbackIfError())
        return false;

    // As we created or updated bookmark - there is no need to create it next time.
    bookmarkOperation = QnCameraBookmarksManager::BookmarkOperation::update;

    if (bookmark.shareable() && NX_ASSERT(!fullId.isEmpty()))
        shareBookmarkLink(fullId);

    emit q->bookmarkChanged();

    return true;
}

//--------------------------------------------------------------------------------------------------

void ShareBookmarkBackend::registerQmlType()
{
    qmlRegisterType<ShareBookmarkBackend>("nx.vms.client.mobile", 1, 0, "ShareBookmarkBackend");

    qmlRegisterSingletonType<detail::BookmarkConstants>(
        "nx.vms.client.mobile", 1, 0, "BookmarkConstants",
        [](QQmlEngine*, QJSEngine*) -> QObject*
        {
            return new detail::BookmarkConstants();
        });
}

ShareBookmarkBackend::ShareBookmarkBackend(QObject* parent):
    base_type(parent),
    d{new Private(this)}
{
    const auto updateAvailablity =
        [this]()
        {
            const auto newValue =
                [this]()
                {
                    const auto camera = d->resource().dynamicCast<QnVirtualCameraResource>();
                    if (!camera)
                        return false;

                    const auto context = d->mobileSystemContext();
                    if (!NX_ASSERT(context))
                        return false;

                    const auto manager = context->windowContext()->sessionManager();
                    const auto currentUser = context->userWatcher()->user();
                    const bool hasManageBookmarksPermission =
                        context->resourceAccessManager()->hasAccessRights(
                            currentUser, d->resource(), api::AccessRight::manageBookmarks);
                    return manager
                        && manager->hasConnectedSession()
                        && manager->isCloudSession()
                        && manager->connectedServerVersion() >= utils::SoftwareVersion(6, 1)
                        && context->moduleInformation().organizationId
                        && hasManageBookmarksPermission;
                }();

            if (d->isAvailable == newValue)
                return;

            d->isAvailable = newValue;
            emit isAvailableChanged();
        };

    updateAvailablity();

    connect(d.data(), &SystemContextAccessor::systemContextIsAboutToBeChanged, this,
        [this]()
        {
            if (const auto systemContext = d->mobileSystemContext())
            {
                systemContext->windowContext()->sessionManager()->disconnect(this);
                systemContext->disconnect(this);
            }
        });

    connect(d.data(), &SystemContextAccessor::systemContextChanged, this,
        [this, updateAvailablity]()
        {
            if (const auto context = d->mobileSystemContext())
            {
                const auto manager = context->windowContext()->sessionManager();
                connect(manager, &SessionManager::sessionHostChanged,
                    this, updateAvailablity);
                connect(manager, &SessionManager::hasConnectedSessionChanged,
                    this, updateAvailablity);
                connect(manager, &SessionManager::connectedServerVersionChanged,
                    this, updateAvailablity);
            }

            updateAvailablity();
        });
}

ShareBookmarkBackend::~ShareBookmarkBackend()
{
}

QModelIndex ShareBookmarkBackend::modelIndex() const
{
    return d->modelIndex;
}

void ShareBookmarkBackend::setModelIndex(const QModelIndex& value)
{
    if (d->modelIndex == value)
        return;

    d->modelIndex = value;

    d->handleIndexUpdated();

    emit modelIndexChanged();
}

bool ShareBookmarkBackend::isAvailable()
{
    return d->isAvailable;
}

bool ShareBookmarkBackend::isShared() const
{
    return isSharedNow();
}

QString ShareBookmarkBackend::bookmarkName() const
{
    return d->bookmark.name;
}

void ShareBookmarkBackend::setBookmarkName(const QString& value)
{
    if (value == d->bookmark.name)
        return;

    d->bookmark.name = value;
    emit bookmarkChanged();
}

QString ShareBookmarkBackend::bookmarkDescription() const
{
    return d->bookmark.description;
}

void ShareBookmarkBackend::setBookmarkDescription(const QString& value)
{
    if (value == d->bookmark.description)
        return;

    d->bookmark.description = value;
    emit bookmarkChanged();
}

qint64 ShareBookmarkBackend::expirationTimeMs() const
{
    return d->bookmark.share.expirationTimeMs.count();
}

QString ShareBookmarkBackend::bookmarkDigest() const
{
    return d->bookmark.share.digest
        ? *d->bookmark.share.digest
        : QString{};
}

bool ShareBookmarkBackend::isSharedNow() const
{
    return d->modelIndex.data(core::IsSharedBookmark).toBool();
}

bool ShareBookmarkBackend::isNeverExpires() const
{
    return !d->bookmark.shareable() || !d->bookmark.bookmarkMatchesFilter(
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::hasExpiration);
}

QString ShareBookmarkBackend::expiresInText() const
{
    if (!NX_ASSERT(d->bookmark.share.expirationTimeMs > qnSyncTime->value()))
        return {};

    const auto timeString =
        [this]()
        {
            const auto duration = d->bookmark.share.expirationTimeMs - qnSyncTime->value();
            if (duration < hours(1))
                return text::HumanReadable::timeSpan(duration, text::HumanReadable::Minutes);
            if (duration < days(1))
                return text::HumanReadable::timeSpan(duration, text::HumanReadable::Hours);
            if (duration < months(1))
                return text::HumanReadable::timeSpan(duration, text::HumanReadable::Days);
            return QString{};
        }();

    return timeString.isEmpty()
        ? QString{}
        : tr("Expires in %1", "%1 is time text like '48 minutes'").arg(timeString);
}

bool ShareBookmarkBackend::share(qint64 expirationTime, const QString& password)
{
    milliseconds expirationTimeMs = {};
    switch (expirationTime)
    {
        case static_cast<qint64>(ExpiresIn::Hour):
            expirationTimeMs = qnSyncTime->value() + hours(1);
            break;
        case static_cast<qint64>(ExpiresIn::Day):
            expirationTimeMs = qnSyncTime->value() + days(1);
            break;
        case static_cast<qint64>(ExpiresIn::Month):
            expirationTimeMs = qnSyncTime->value() + months(1);
            break;
        case static_cast<qint64>(ExpiresIn::Never):
            expirationTimeMs = 0ms;
            break;
        default:
            expirationTimeMs= milliseconds(expirationTime);
            break;
    }

    const auto context = d->systemContext();
    if (!NX_ASSERT(context))
        return false;

    const auto shareParams = common::BookmarkShareableParams{
        .shareable = true,
        .expirationTimeMs = expirationTimeMs,
        .digest = password == d->bookmark.share.digest
            ? std::optional<QString>{} //< Changes nothing on patch request.
            : password
    };

    return d->updateShareParams(shareParams);
}

bool ShareBookmarkBackend::stopSharing()
{
    return NX_ASSERT(d->bookmarkOperation != QnCameraBookmarksManager::BookmarkOperation::create)
        && d->updateShareParams({.shareable = false, .expirationTimeMs = 0ms, .digest = ""});
}

} // namespace nx::vms::client::mobile
