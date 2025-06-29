// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_event_list_model.h"

#include <QtGui/QPixmap>

#include <core/resource/resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/event_search/utils/event_search_utils.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

static const QColor kLight12Color = "#91A7B2";
static const QColor kLight10Color = "#A5B7C0";
static const SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QnIcon::Normal, {{kLight12Color, "light12"}, {kLight10Color, "light10"}}},
};

} // namespace

struct AbstractEventListModel::Private
{
    AbstractEventListModel* q;

    QPointer<SystemContext> systemContext;
};

AbstractEventListModel::AbstractEventListModel(
    QObject* parent)
    :
    base_type(parent),
    d(new Private({.q = this}))
{
}

AbstractEventListModel::~AbstractEventListModel()
{
}

SystemContext* AbstractEventListModel::systemContext() const
{
    return d->systemContext.data();
}

void AbstractEventListModel::setSystemContext(SystemContext* systemContext)
{
    if (systemContext == this->systemContext())
        return;

    d->systemContext = systemContext;

    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {TimestampTextRole});
}

QVariant AbstractEventListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case TimestampMsRole:
            return QVariant::fromValue(duration_cast<milliseconds>(
                index.data(TimestampRole).value<microseconds>()).count());

        case TimestampTextRole:
        {
            const auto timestampMs = duration_cast<milliseconds>(
                index.data(TimestampRole).value<microseconds>());

            const auto resource = index.data(ResourceRole).value<QnResourcePtr>();
            const auto context = resource
                ? resource->systemContext()->as<SystemContext>()
                : d->systemContext.data();

            if (NX_ASSERT(context))
                return EventSearchUtils::timestampText(timestampMs, context);
            else
                return QString();
        }

        case DisplayedResourceListRole:
            return index.data(ResourceListRole);

        case Qt::AccessibleTextRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            return index.data(Qt::DisplayRole);

        case Qt::AccessibleDescriptionRole:
            return index.data(DescriptionTextRole);

        case Qt::DecorationRole:
        {
            const auto path = index.data(DecorationPathRole).toString();
            return path.isEmpty()
                ? QPixmap()
                : path.endsWith(".svg")
                    ? qnSkin->icon(path, kIconSubstitutions).pixmap(20)
                    : qnSkin->pixmap(path);
        }

        default:
            return QVariant();
    }
}

bool AbstractEventListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!isValid(index) || index.model() != this)
        return false;

    switch (role)
    {
        case DefaultNotificationRole:
            return defaultAction(index);

        case ActivateLinkRole:
            return activateLink(index, value.toString());

        default:
            return base_type::setData(index, value, role);
    }
}

QHash<int, QByteArray> AbstractEventListModel::roleNames() const
{
    auto result = base_type::roleNames();
    result.insert(clientCoreRoleNames());
    return result;
}

bool AbstractEventListModel::isValid(const QModelIndex& index) const
{
    return index.model() == this
        && !index.parent().isValid()
        && index.column() == 0
        && index.row() >= 0
        && index.row() < rowCount();
}

bool AbstractEventListModel::defaultAction(const QModelIndex& /*index*/)
{
    return false;
}

bool AbstractEventListModel::activateLink(const QModelIndex& index, const QString& /*link*/)
{
    // Default fallback implementation.
    return defaultAction(index);
}

} // namespace nx::vms::client::core
