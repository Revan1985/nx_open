// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_model.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>

namespace nx::vms::client::desktop {

namespace {

const QString kNormalLogLevelColor = "light16";
const QString kWarningLogLevelColor = "yellow";

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kRedSubstitutions = {
    {QIcon::Normal, {.primary = "attention.red"}}
};
static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kGreenSubstitutions = {
    {QIcon::Normal, {.primary = "attention.green"}}
};

NX_DECLARE_COLORIZED_ICON(kCloseIcon, "20x20/Outline/close_medium.svg", kRedSubstitutions)
NX_DECLARE_COLORIZED_ICON(kSuccessIcon, "20x20/Outline/check.svg", kGreenSubstitutions)

QIcon icon(LogsManagementUnitPtr unit)
{
    return unit->server()
        ? qnResIconCache->icon(unit->server())
        : qnResIconCache->icon(QnResourceIconCache::Client);
}

QVariant statusIcon(LogsManagementUnitPtr unit)
{
    using State = LogsManagementWatcher::Unit::DownloadState;
    switch (unit->state())
    {
        case State::pending:
        case State::loading:
            return {};

        case State::complete:
            return qnSkin->icon(kSuccessIcon);

        case State::error:
            return qnSkin->icon(kCloseIcon);
    }

    return {};
}

nx::log::Level logLevel(LogsManagementUnitPtr unit)
{
    if (!unit->settings())
        return nx::log::Level::undefined;

    // Currently we show MainLog level only, even if settings of other Loggers differ.
    return unit->settings()->mainLog.primaryLevel;
}

QColor logLevelColor(LogsManagementUnitPtr unit)
{
    return logLevel(unit) == LogsManagementWatcher::defaultLogLevel()
        ? core::colorTheme()->color(kNormalLogLevelColor)
        : core::colorTheme()->color(kWarningLogLevelColor);
}

bool isOnline(LogsManagementUnitPtr unit)
{
    if (auto server = unit->server())
        return server->isOnline();

    return true; //< The unit is a client.
}

} // namespace

LogsManagementModel::LogsManagementModel(QObject* parent, LogsManagementWatcher* watcher):
    base_type(parent),
    m_watcher(watcher)
{
    // Model uses its own list of shared pointers to LogsManagementUnit instances, which is updated
    // when receiving a signal from Watcher. This list allows to safely show quite consistent data
    // w/o any additional synchronization between Model and Watcher during ResourcePool updates.
    connect(
        m_watcher, &LogsManagementWatcher::itemListChanged,
        this, &LogsManagementModel::onItemsListChanged);

    connect(
        m_watcher, &LogsManagementWatcher::itemsChanged,
        this, &LogsManagementModel::onItemsChanged);

    m_items = watcher->items();
}

int LogsManagementModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int LogsManagementModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_items.size();
}

QVariant LogsManagementModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case NameColumn:
                return tr("Unit");
            case LogLevelColumn:
                return tr("Current Level");
            default:
                break;
        }
    }

    return {};
}

QVariant LogsManagementModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    auto unit = m_items.value(index.row());
    if (!NX_ASSERT(unit))
        return {};

    switch (role)
    {
        case IpAddressRole:
        {
            if (auto server = unit->server())
                return QnResourceDisplayInfo(server).host();

            return {};
        }

        case EnabledRole:
            if (index.column() == CheckBoxColumn)
                break;

            return isOnline(unit)
                && unit->state() != LogsManagementWatcher::Unit::DownloadState::complete;

        case QnUuidRole:
            return unit->id().toString(QUuid::WithBraces);
    }

    switch (index.column())
    {
        case NameColumn:
            switch (role)
            {
                case Qt::DecorationRole:
                    return icon(unit);

                case Qt::DisplayRole:
                    return unit->server()
                        ? QnResourceDisplayInfo(unit->server()).name()
                        : tr("Client");
            }
            break;

        case LogLevelColumn:
            switch (role)
            {
                case Qt::DisplayRole:
                    return isOnline(unit) ? logLevelName(logLevel(unit)) : "";

                case Qt::ForegroundRole:
                    return logLevelColor(unit);

                case Qt::ToolTipRole:
                    return isOnline(unit) ? logLevelTooltip(logLevel(unit)) : "";
            }
            break;

        case CheckBoxColumn:
            switch (role)
            {
                case Qt::CheckStateRole:
                    return unit->isChecked() ? Qt::Checked : Qt::Unchecked;

                case EnabledRole:
                    auto watcherState = m_watcher->state();
                    if (watcherState == LogsManagementWatcher::State::empty
                        || watcherState == LogsManagementWatcher::State::hasSelection)
                    {
                        return unit->state() == LogsManagementWatcher::Unit::DownloadState::none;
                    }
                    return  false;
            }
            break;

        case StatusColumn:
            switch (role)
            {
                case Qt::DecorationRole:
                    return statusIcon(unit);
                case DownloadingRole:
                {
                    auto state = unit->state();
                    return state == LogsManagementWatcher::Unit::DownloadState::pending
                        || state == LogsManagementWatcher::Unit::DownloadState::loading;
                }
            }
            break;

        case RetryColumn:
            switch (role)
            {
                case RetryRole:
                    return unit->state() == LogsManagementWatcher::Unit::DownloadState::error;
            }
            break;

        default:
            NX_ASSERT(false, "Unknown column id");
            break;
    }

    return {};
}

bool LogsManagementModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (index.column() != CheckBoxColumn || role != Qt::CheckStateRole)
        return false;

    if (!m_watcher)
        return false;

    auto item = m_items.value(index.row());
    auto state = static_cast<Qt::CheckState>(value.toInt());

    // Offline servers can't be checked, but it should be possible to uncheck a server that went
    // offline after it was checked by user.
    if (!isOnline(item))
        state = Qt::Unchecked;

    m_watcher->setItemIsChecked(item, state == Qt::Checked);
    return true;
}

QList<nx::log::Level> LogsManagementModel::logLevels()
{
    using Level = nx::log::Level;
    return {Level::none, Level::error, Level::warning, Level::info, Level::debug, Level::verbose};
}

QString LogsManagementModel::logLevelName(nx::log::Level level)
{
    switch (level)
    {
        case nx::log::Level::none: return tr("<None>");
        case nx::log::Level::error: return tr("Error");
        case nx::log::Level::warning: return tr("Warning");
        case nx::log::Level::info: return tr("Info");
        case nx::log::Level::debug: return tr("Debug");
        case nx::log::Level::verbose: return tr("Verbose");
        default: return {};
    }
}

void LogsManagementModel::onItemsListChanged()
{
    beginResetModel();
    m_items = m_watcher->items();
    if (!m_filterText.isEmpty())
    {
        for (int i = 0; i < m_items.size();)
        {
            if (m_items[i]->server().isNull())
            {
                if (QString{"client"}.contains(m_filterText.toLower()))
                    ++i;
                else
                    m_items.remove(i);
            }
            else
            {
                if (m_items[i]->server()->getName().toLower().contains(m_filterText.toLower()))
                    ++i;
                else
                    m_items.remove(i);
            }
        }
    }
    endResetModel();
}

void LogsManagementModel::onItemsChanged(QList<LogsManagementUnitPtr> items)
{
    QList<nx::Uuid> ids;
    for (const auto& item: items)
    {
        ids << item->id();
    }

    int first = -1, last = -1;
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (ids.contains(m_items[i]->id()))
        {
            if (first < 0)
                first = i;
            last = i;
        }
    }

    if (first >= 0)
    {
        // Redraw items.
        emit dataChanged(index(first, 0), index(last, ColumnCount - 1));
    }
}

QString LogsManagementModel::logLevelTooltip(nx::log::Level level) const
{
    switch (level)
    {
        case nx::log::Level::undefined:
            return {};
        case nx::log::Level::info:
            return tr("Default Logging level");
        case nx::log::Level::none:
        case nx::log::Level::error:
        case nx::log::Level::warning:
            return tr("Non-default Logging level. We recommend setting it to \"info\"");
        case nx::log::Level::debug:
        case nx::log::Level::verbose:
            return tr("Logging level degrades performance");
    }

    NX_ASSERT(false, "Unexpected value (%1)", level);
    return {};
}

void LogsManagementModel::setFilterText(const QString& text)
{
    m_filterText = text;
    onItemsListChanged();
}

} // namespace nx::vms::client::desktop
