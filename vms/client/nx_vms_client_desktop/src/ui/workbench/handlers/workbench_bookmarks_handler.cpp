// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_bookmarks_handler.h"

#include <chrono>

#include <QtGui/QAction>

#include <api/common_message_processor.h>
#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/bookmarks/bookmark_tags_watcher.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <recording/time_period.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/synctime.h>

using std::chrono::microseconds;

using namespace nx::vms::client::desktop;

QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = nullptr */):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(menu::AddCameraBookmarkAction),     &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered);
    connect(action(menu::EditCameraBookmarkAction),    &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered);
    connect(action(menu::RemoveBookmarkAction),  &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered);
    connect(action(menu::RemoveBookmarksAction),       &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_removeBookmarksAction_triggered);
    connect(action(menu::BookmarksModeAction),         &QAction::toggled,   this,
        &QnWorkbenchBookmarksHandler::at_bookmarksModeAction_triggered);

    const auto getActionParamsFunc =
        [this](const QnCameraBookmark &bookmark) -> menu::Parameters
        {
            return navigator()->currentParameters(menu::TimelineScope)
                .withArgument(nx::vms::client::core::CameraBookmarkRole, bookmark);
        };

    const QPointer<QnBookmarksViewer> bookmarksViewer(navigator()->timeSlider()->bookmarksViewer());

    connect(bookmarksViewer, &QnBookmarksViewer::editBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            statisticsModule()->controls()->registerClick("bookmark_tooltip_edit");
            menu()->triggerIfPossible(menu::EditCameraBookmarkAction,
                getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::removeBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            statisticsModule()->controls()->registerClick("bookmark_tooltip_delete");
            menu()->triggerIfPossible(menu::RemoveBookmarkAction,
                getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::exportBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            statisticsModule()->controls()->registerClick("bookmark_tooltip_export");
            menu()->triggerIfPossible(menu::ExportBookmarkAction, getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::playBookmark, this,
        [this](const QnCameraBookmark &bookmark)
        {
            statisticsModule()->controls()->registerClick("bookmark_tooltip_play");

            auto slider = navigator()->timeSlider();

            // Pretty bookmark navigation should be performed when the slider is not immediately visible
            // to the user (because either live streaming or the slider is outside of the time window).
            const bool isVisibleInWindow = slider->positionMarkerVisible() &&
                slider->windowContains(slider->sliderTimePosition());

            navigator()->setPosition(microseconds(bookmark.startTimeMs).count());
            if (!isVisibleInWindow)
                slider->navigateTo(bookmark.startTimeMs);

            navigator()->setPlaying(true);
        });

    connect(bookmarksViewer, &QnBookmarksViewer::tagClicked, this,
        [this, bookmarksViewer](const QString &tag)
        {
            statisticsModule()->controls()->registerClick("bookmark_tooltip_tag");
            menu()->triggerIfPossible(menu::OpenBookmarksSearchAction,
                menu::Parameters().withArgument(nx::vms::client::core::BookmarkTagRole, tag));
        });
}

QnWorkbenchBookmarksHandler::~QnWorkbenchBookmarksHandler()
{
}

void QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    // TODO: #sivanov Will we support these actions for exported layouts?
    if (!camera || !camera->systemContext())
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    QnCameraBookmark bookmark;

    // This should be assigned before loading data to the dialog.
    bookmark.guid = nx::Uuid::createUuid();
    bookmark.name = tr("Bookmark");
    bookmark.startTimeMs = period.startTime();
    bookmark.durationMs = period.duration();
    bookmark.cameraId = camera->getId();

    auto dialog = createSelfDestructingDialog<QnCameraBookmarkDialog>(false, mainWindowWidget());
    dialog->setTags(systemContext()->bookmarkTagWatcher()->tags());
    dialog->loadData(bookmark);

    connect(
        dialog,
        &QnCameraBookmarkDialog::accepted,
        this,
        [this, dialog, bookmark = std::move(bookmark)]() mutable
        {
            const auto context = dialog->context();

            bookmark.creatorId = context->user()->getId();
            bookmark.creationTimeStampMs = qnSyncTime->value();
            dialog->submitData(bookmark);
            if (!NX_ASSERT(bookmark.isValid(), "Dialog must not allow to create invalid bookmarks"))
                return;

            context->system()->cameraBookmarksManager()->addCameraBookmark(bookmark);

            action(menu::BookmarksModeAction)->setChecked(true);
        });

    dialog->show();
}

void QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    // TODO: #sivanov Will we support these actions for exported layouts?
    if (!camera || !camera->systemContext())
        return;

    QnCameraBookmark bookmark = parameters.argument<QnCameraBookmark>(
        nx::vms::client::core::CameraBookmarkRole);

    QnMediaServerResourcePtr server = cameraHistoryPool()->getMediaServerOnTime(camera,
        bookmark.startTimeMs.count());
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Server offline"),
            tr("Bookmarks can only be edited on an online Server."));
        return;
    }

    auto dialog = createSelfDestructingDialog<QnCameraBookmarkDialog>(false, mainWindowWidget());

    dialog->setTags(systemContext()->bookmarkTagWatcher()->tags());
    dialog->loadData(bookmark);

    connect(
        dialog,
        &QnCameraBookmarkDialog::accepted,
        this,
        [dialog, bookmark = std::move(bookmark)]() mutable
        {
            dialog->submitData(bookmark);
            dialog->context()->system()->cameraBookmarksManager()->updateCameraBookmark(bookmark);
        });

    dialog->show();
}

void QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    // TODO: #sivanov Will we support these actions for exported layouts?
    if (!camera || !camera->systemContext())
        return;

    QnCameraBookmark bookmark = parameters.argument<QnCameraBookmark>(
        nx::vms::client::core::CameraBookmarkRole);

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Delete bookmark?"), bookmark.name.trimmed(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindowWidget());
    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    const auto context = nx::vms::client::core::SystemContext::fromResource(camera);
    context->cameraBookmarksManager()->deleteCameraBookmark(bookmark.guid);
}

void QnWorkbenchBookmarksHandler::at_removeBookmarksAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnCameraBookmarkList bookmarks = parameters.argument<QnCameraBookmarkList>(Qn::CameraBookmarkListRole);
    if (bookmarks.empty())
        return;

    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Delete %n bookmarks?", "", bookmarks.size()), QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        parent);
    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    // FIXME: #sivanov Actual system context probably should be linked to the bookmarks.
    const auto context = nx::vms::client::desktop::appContext()->currentSystemContext();
    const auto bookmarksManager = context->cameraBookmarksManager();
    for (const auto& bookmark: bookmarks)
        bookmarksManager->deleteCameraBookmark(bookmark.guid);
}

void QnWorkbenchBookmarksHandler::at_bookmarksModeAction_triggered()
{
    navigator()->setBookmarksModeEnabled(action(menu::BookmarksModeAction)->isChecked());
}
