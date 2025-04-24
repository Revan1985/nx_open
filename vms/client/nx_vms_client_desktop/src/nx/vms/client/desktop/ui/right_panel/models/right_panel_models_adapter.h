// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QVector>
#include <QtGui/QColor>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/fetch_request.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>

Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/analytics_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/analytics/attribute_display_manager.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/event_rules/models/detectable_object_type_model.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/event_search/utils/common_object_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/window_context.h")

namespace nx::analytics::db { struct ObjectTrack; }

namespace nx::vms::client::core { class AnalyticsSearchSetup; }

namespace nx::vms::client::core::analytics::taxonomy { class AnalyticsFilterModel; }

namespace nx::vms::client::desktop {

class CommonObjectSearchSetup;
class DetectableObjectTypeModel;
class WindowContext;

class RightPanelModelsAdapter: public QIdentityProxyModel
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context
        READ context WRITE setContext NOTIFY contextChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)

    Q_PROPERTY(bool isAllowed READ isAllowed NOTIFY allowanceChanged)

    Q_PROPERTY(nx::vms::client::core::EventSearch::SearchType type
        READ type
        WRITE setType
        NOTIFY typeChanged)

    Q_PROPERTY(nx::vms::client::desktop::CommonObjectSearchSetup* commonSetup READ commonSetup
        CONSTANT)

    Q_PROPERTY(nx::vms::client::core::AnalyticsSearchSetup* analyticsSetup READ analyticsSetup
        NOTIFY analyticsSetupChanged)

    Q_PROPERTY(nx::vms::client::core::analytics::taxonomy::AnalyticsFilterModel*
        analyticsFilterModel READ analyticsFilterModel NOTIFY analyticsSetupChanged)

    Q_PROPERTY(nx::vms::client::desktop::DetectableObjectTypeModel*
        objectTypeModel READ objectTypeModel NOTIFY analyticsSetupChanged)

    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(QString itemCountText READ itemCountText NOTIFY itemCountChanged)
    Q_PROPERTY(bool isConstrained READ isConstrained NOTIFY isConstrainedChanged)

    Q_PROPERTY(bool unreadTracking READ unreadTrackingEnabled WRITE setUnreadTrackingEnabled
        NOTIFY unreadTrackingChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(QColor unreadColor READ unreadColor NOTIFY unreadCountChanged)

    Q_PROPERTY(bool placeholderRequired READ isPlaceholderRequired
        NOTIFY placeholderRequiredChanged)

    Q_PROPERTY(bool previewsEnabled READ previewsEnabled WRITE setPreviewsEnabled
        NOTIFY previewsEnabledChanged)

    /**
     * Whether this model is currently active.
     * Activity can be changed from outside (e.g. by user choosing a corresponding UI tab)
     * or from inside (for example by toggling Motion Search mode).
     */
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

    Q_PROPERTY(QVector<RightPanel::VmsEventGroup> eventGroups READ eventGroups
        NOTIFY eventGroupsChanged)
    Q_PROPERTY(QAbstractItemModel* analyticsEvents READ analyticsEvents
        NOTIFY analyticsEventsChanged)

    Q_PROPERTY(bool crossSiteMode READ crossSiteMode NOTIFY crossSiteModeChanged)

    using base_type = QIdentityProxyModel;

public:
    RightPanelModelsAdapter(QObject* parent = nullptr);
    virtual ~RightPanelModelsAdapter() override;

    WindowContext* context() const;
    void setContext(WindowContext* value);

    core::EventSearch::SearchType type() const;
    void setType(core::EventSearch::SearchType value);

    bool isOnline() const;
    bool isAllowed() const;

    bool active() const;
    void setActive(bool value);

    CommonObjectSearchSetup* commonSetup() const;
    core::AnalyticsSearchSetup* analyticsSetup() const;

    nx::vms::client::core::analytics::taxonomy::AnalyticsFilterModel* analyticsFilterModel() const;
    DetectableObjectTypeModel* objectTypeModel() const;

    int itemCount() const;
    QString itemCountText() const;
    bool isConstrained() const;
    bool isPlaceholderRequired() const;

    bool unreadTrackingEnabled() const;
    void setUnreadTrackingEnabled(bool value);
    int unreadCount() const;
    QColor unreadColor() const;

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool value);

    Q_INVOKABLE bool removeRow(int row);

    Q_INVOKABLE void setAutoClosePaused(int row, bool value);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void fetchData(
        const core::FetchRequest& request,
        bool immediately = false);

    Q_INVOKABLE bool fetchInProgress() const;

    Q_INVOKABLE void setLivePaused(bool value);

    Q_INVOKABLE void click(int row, int button, int modifiers);
    Q_INVOKABLE void doubleClick(int row);
    Q_INVOKABLE void startDrag(int row, const QPoint& pos, const QSize& size);
    Q_INVOKABLE void activateLink(int row, const QString& link);
    Q_INVOKABLE void showContextMenu(int row, const QPoint& globalPos,
        bool withStandardInteraction);

    Q_INVOKABLE void addCameraToLayout();

    Q_INVOKABLE void showEventLog();

    Q_INVOKABLE void showOnLayout(int row);

    QVector<RightPanel::VmsEventGroup> eventGroups() const;
    QAbstractItemModel* analyticsEvents() const;

    void setHighlightedTimestamp(std::chrono::microseconds value);
    void setHighlightedResources(const QSet<QnResourcePtr>& value);

    Q_INVOKABLE core::FetchRequest requestForDirection(
        core::EventSearch::FetchDirection direction);

    bool crossSiteMode() const;

    static void registerQmlTypes();

signals:
    void contextChanged();
    void typeChanged();
    void dataNeeded();
    void asyncFetchStarted(const nx::vms::client::core::FetchRequest& request);
    void fetchCommitStarted(const nx::vms::client::core::FetchRequest& request);
    void fetchFinished(core::EventSearch::FetchResult result,
        int centralItemIndex,
        const core::FetchRequest& request);
    void analyticsSetupChanged();
    void itemCountChanged();
    void isConstrainedChanged();
    void isOnlineChanged();
    void activeChanged();
    void placeholderRequiredChanged();
    void previewsEnabledChanged();
    void allowanceChanged();
    void eventGroupsChanged();
    void analyticsEventsChanged();
    void unreadTrackingChanged();
    void unreadCountChanged();

    void clicked(const QModelIndex& index, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void doubleClicked(const QModelIndex& index);
    void dragStarted(const QModelIndex& index, const QPoint& pos, const QSize& size);
    void linkActivated(const QModelIndex& index, const QString& link);
    void contextMenuRequested(
        const QModelIndex& index,
        const QPoint& globalPos,
        bool withStandardInteraction,
        QWidget* parent);
    void pluginActionRequested(const nx::Uuid& engineId, const QString& actionTypeId,
        const nx::analytics::db::ObjectTrack& track, const QnVirtualCameraResourcePtr& camera);
    void livePausedChanged(bool isPaused);
    void crossSiteModeChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
