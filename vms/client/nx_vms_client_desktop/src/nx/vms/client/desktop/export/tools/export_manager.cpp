// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_manager.h"

#include <QtCore/QPointer>

#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/export/tools/export_media_tool.h>
#include <nx/utils/log/log.h>

#include "export_layout_tool.h"

namespace nx::vms::client::desktop {

ExportProcess::ExportProcess(const nx::Uuid& id, std::unique_ptr<AbstractExportTool>&& tool, QObject* parent):
    base_type(parent),
    m_info(id),
    m_tool(std::move(tool))
{
    connect(m_tool.get(), &ExportMediaTool::rangeChanged, this,
        [this](int from, int to)
        {
            m_info.rangeStart = from;
            m_info.rangeEnd = to;
            m_info.progressValue = qBound(from, m_info.progressValue, to);
            emit infoChanged(m_info);
        });

    connect(m_tool.get(), &ExportMediaTool::valueChanged, this,
        [this](int value)
        {
            m_info.progressValue = qBound(m_info.rangeStart, value, m_info.rangeEnd);
            emit infoChanged(m_info);
        });

    connect(m_tool.get(), &ExportMediaTool::statusChanged, this,
        [this](ExportProcessStatus status)
        {
            m_info.status = status;
            m_info.error = m_tool->lastError();
            emit infoChanged(m_info);
        });

    connect(m_tool.get(), &ExportMediaTool::finished, this,
        [this]
        {
            emit finished(m_info.id);
        });
}

ExportProcess::~ExportProcess()
{
}

const ExportProcessInfo& ExportProcess::info() const
{
    return m_info;
}

void ExportProcess::start()
{
    m_tool->start();
}

void ExportProcess::stop()
{
    m_tool->stop();
}

struct ExportManager::Private
{
    ExportManager* const q;
    QMap<nx::Uuid, QPointer<ExportProcess>> exportProcesses;

    explicit Private(ExportManager* owner):
        q(owner)
    {
    }

    nx::Uuid startExport(ExportProcess* process)
    {
        connect(process, &ExportProcess::infoChanged, q, &ExportManager::processUpdated);
        connect(process, &ExportProcess::finished, q,
            [this](const nx::Uuid& id)
            {
                if (auto process = exportProcesses.take(id))
                {
                    emit q->processFinished(process->info());
                    process->deleteLater();
                }
            });
        exportProcesses.insert(process->info().id, process);
        process->start();
        return process->info().id;
    }

    void stopExport(const nx::Uuid& exportProcessId)
    {
        if (const auto process = exportProcesses.value(exportProcessId))
            process->stop();
    }

    ExportProcessInfo info(const nx::Uuid& exportProcessId) const
    {
        if (const auto process = exportProcesses.value(exportProcessId))
            return process->info();
        return ExportProcessInfo(nx::Uuid());
    }
};

ExportManager::ExportManager(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

ExportManager::~ExportManager()
{
    while (!d->exportProcesses.empty())
    {
        auto process = d->exportProcesses.take(d->exportProcesses.lastKey());
        if (process)
            process->stop();
    }
}

nx::Uuid ExportManager::startExport(const nx::Uuid& id, std::unique_ptr<AbstractExportTool>&& tool)
{
    NX_VERBOSE(this, "Export process id: %1", id.toSimpleString());
    return d->startExport(new ExportProcess(id, std::move(tool), this));
}

void ExportManager::stopExport(const nx::Uuid& exportProcessId)
{
    NX_VERBOSE(this, "Stopped export process id: %1", exportProcessId.toString());
    d->stopExport(exportProcessId);
}

ExportProcessInfo ExportManager::info(const nx::Uuid& exportProcessId) const
{
    return d->info(exportProcessId);
}

QString ExportProcess::errorString(ExportProcessError error)
{
    // TODO: #sivanov Better texts.
    switch (error)
    {
        case ExportProcessError::noError:
            return QString();

        case ExportProcessError::unsupportedMedia:
            return tr("Unsupported media for data export.");

        case ExportProcessError::unsupportedFormat:
            return tr("Selected format is not supported by FFMPEG library.");

        case ExportProcessError::ffmpegError:
            return tr("FFMPEG library error.");

        case ExportProcessError::incompatibleCodec:
            return tr("Video or audio codec is incompatible with selected format.");

        case ExportProcessError::videoTranscodingRequired:
            return tr("Video transcoding required.");

        case ExportProcessError::audioTranscodingRequired:
            return tr("Audio transcoding required.");

        case ExportProcessError::fileAccess:
            return tr("File write error.");

        case ExportProcessError::dataNotFound:
            return tr("No data exported.");

        case ExportProcessError::encryptedArchive:
            return tr("Unlock this portion of the archive to export its contents.");

        case ExportProcessError::temporaryUnavailable:
            return tr("Archive is unavailable now. Please try again later.");

        default:
            NX_ASSERT(false, "Should never get here");
            return tr("Internal error");
    }
}

} // namespace nx::vms::client::desktop
