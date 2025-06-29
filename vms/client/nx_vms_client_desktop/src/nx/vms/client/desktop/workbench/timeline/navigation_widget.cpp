// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "navigation_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include "clock_label.h"
#include "speed_slider.h"

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

bool paintButtonFunction(QPainter* painter, const QStyleOption* /*option*/, const QWidget* widget)
{
    const QPushButton* thisButton = qobject_cast<const QPushButton*>(widget);

    QIcon::Mode mode = QnIcon::Normal;

    if (!thisButton->isEnabled())
        mode = QnIcon::Disabled;
    else if (thisButton->isDown())
        mode = QnIcon::Pressed;
    else if (thisButton->underMouse())
        mode = QnIcon::Active;

    thisButton->icon().paint(painter, thisButton->rect(), Qt::AlignCenter,
        mode, thisButton->isChecked() ? QIcon::On : QIcon::Off);

    return true;
};

const core::SvgIconColorer::ThemeSubstitutions kNavigationIconSubstitutions = {
    {QIcon::Normal, {.primary = "light4", .secondary = "dark7"}},
    {QIcon::Active, {.primary = "light6", .secondary = "dark8"}},
    {QIcon::Disabled, {.primary = "light4", .secondary = "dark7", .alpha = 0.3}},
    {QnIcon::Pressed, {.primary = "light2", .secondary = "dark6"}},
};

NX_DECLARE_COLORIZED_ICON(
    kJumpBackwardIcon, "32x32/Solid/rewind_backward.svg", kNavigationIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(
    kStepBackwardIcon, "32x32/Solid/step_backward.svg", kNavigationIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kPlayPauseIcon,
    "32x32/Solid/play.svg",
    kNavigationIconSubstitutions,
    "32x32/Solid/pause.svg",
    kNavigationIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(
    kStepForwardIcon, "32x32/Solid/step_forward.svg", kNavigationIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(
    kJumpForwardIcon, "32x32/Solid/rewind_forward.svg", kNavigationIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kBackwadIcon, "32x32/Solid/backward.svg", kNavigationIconSubstitutions)
NX_DECLARE_COLORIZED_ICON(kForwardIcon, "32x32/Solid/forward.svg", kNavigationIconSubstitutions)

} // namespace

NavigationWidget::NavigationWidget(WindowContext* context, QWidget* parent):
    QWidget(parent),
    WindowContextAware(context),
    m_speedSlider(new SpeedSlider(context, this)),
    m_jumpBackwardButton(new CustomPaintedButton(this)),
    m_stepBackwardButton(new CustomPaintedButton(this)),
    m_playButton(new CustomPaintedButton(this)),
    m_stepForwardButton(new CustomPaintedButton(this)),
    m_jumpForwardButton(new CustomPaintedButton(this))
{
    m_rewindDisabledText = tr("Rewind is not available for VMAX");
    m_speedSlider->setRestrictSpeed(0.0);

    installEventHandler({this}, {QEvent::Resize, QEvent::Move},
        this, &NavigationWidget::geometryChanged);

    initButton(m_jumpBackwardButton, menu::JumpToStartAction, kJumpBackwardIcon);
    initButton(m_stepBackwardButton, menu::PreviousFrameAction, kStepBackwardIcon);
    initButton(m_playButton, menu::PlayPauseAction, kPlayPauseIcon);
    initButton(m_stepForwardButton, menu::NextFrameAction, kStepForwardIcon);
    initButton(m_jumpForwardButton, menu::JumpToEndAction, kJumpForwardIcon);

    auto mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(3);
    setLayout(mainLayout);

    statisticsModule()->controls()->registerSlider(
        "speed_slider",
        m_speedSlider);
    mainLayout->addWidget(m_speedSlider);

    auto buttonsLayout = new QHBoxLayout();
    mainLayout->addLayout(buttonsLayout);

    buttonsLayout->setSpacing(1);

    buttonsLayout->addWidget(m_jumpBackwardButton);
    buttonsLayout->addWidget(m_stepBackwardButton);
    buttonsLayout->addWidget(m_playButton);
    buttonsLayout->addWidget(m_stepForwardButton);
    buttonsLayout->addWidget(m_jumpForwardButton);

    mainLayout->addWidget(new ClockLabel(this));

    /* Set up handlers. */
    connect(m_speedSlider, &SpeedSlider::roundedSpeedChanged, this,
        [this]
        {
            updateNavigatorSpeedFromSpeedSlider();
            updateBackwardButtonEnabled();
        });
    connect(m_speedSlider, &SpeedSlider::wheelMoved, this,
        [this](bool forwardDirection)
        {
            if (forwardDirection)
                m_stepForwardButton->click();
            else
                m_stepBackwardButton->click();
        });

    connect(m_speedSlider, &SpeedSlider::sliderRestricted, this,
        [this]
        {
            showMessage(m_rewindDisabledText);
        });

    connect(action(menu::ToggleSyncAction), &QAction::toggled, this,
        [this]
        {
            updateBackwardButtonEnabled();
        },
        Qt::QueuedConnection);
    connect(action(menu::PreviousFrameAction), &QAction::triggered,
        this, &NavigationWidget::at_stepBackwardButton_clicked);
    connect(action(menu::NextFrameAction), &QAction::triggered,
        this, &NavigationWidget::at_stepForwardButton_clicked);
    connect(action(menu::JumpToStartAction), &QAction::triggered,
        navigator(), &QnWorkbenchNavigator::jumpBackward);
    connect(action(menu::JumpToEndAction), &QAction::triggered,
        navigator(), &QnWorkbenchNavigator::jumpForward);
    connect(action(menu::PlayPauseAction), &QAction::toggled,
        navigator(), &QnWorkbenchNavigator::setPlaying);
    connect(action(menu::PlayPauseAction), &QAction::toggled,
        this, &NavigationWidget::updateSpeedSliderParametersFromNavigator);
    connect(action(menu::PlayPauseAction), &QAction::toggled, this,
        [this]
        {
            updatePlaybackButtonsIcons();
            updatePlaybackButtonsEnabled();
        });

    // Play button is not synced with the actual playing state, so we update it only when current
    // widget changes.
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &NavigationWidget::updatePlayButtonChecked, Qt::QueuedConnection);

    connect(navigator(), &QnWorkbenchNavigator::currentLayoutItemRemoved,
        this, &NavigationWidget::updateBackwardButtonEnabled);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged,
        m_speedSlider, &SpeedSlider::finishAnimations);
    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &NavigationWidget::updateJumpButtonsTooltips);
    connect(navigator(), &QnWorkbenchNavigator::liveChanged,
        this, &NavigationWidget::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::playingSupportedChanged,
        this, &NavigationWidget::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged,
        this, &NavigationWidget::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::timelineRelevancyChanged,
        this, &NavigationWidget::updatePlaybackButtonsEnabled);
    connect(navigator(), &QnWorkbenchNavigator::speedChanged,
        this, &NavigationWidget::updateSpeedSliderSpeedFromNavigator);
    connect(navigator(), &QnWorkbenchNavigator::speedRangeChanged,
        this, &NavigationWidget::updateSpeedSliderParametersFromNavigator);
    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged,
        this, &NavigationWidget::updateSpeedSliderParametersFromNavigator);

    updatePlaybackButtonsEnabled();
    updatePlaybackButtonsIcons();
    updateSpeedSliderParametersFromNavigator();
    updateSpeedSliderSpeedFromNavigator();
}

void NavigationWidget::setTooltipsVisible(bool enabled)
{
    m_speedSlider->setTooltipsVisible(enabled);
}

void NavigationWidget::initButton(
    CustomPaintedButton* button,
    menu::IDType actionType,
    const nx::vms::client::core::ColorizedIconDeclaration& iconDecl,
    const QString& checkedIconPath)
{
    const bool isCheckable = !iconDecl.checkedThemeSubstitutions().isEmpty();
    QAction* buttonAction = action(actionType);

    button->setCustomPaintFunction(paintButtonFunction);
    button->setIcon(qnSkin->icon(iconDecl));
    button->setFixedSize({32, 32});
    button->setToolTip(buttonAction->toolTip());
    button->setCheckable(isCheckable);
    button->setObjectName(buttonAction->text());

    if (isCheckable)
    {
        connect(button, &QPushButton::toggled, buttonAction, &QAction::setChecked);
        connect(
            buttonAction,
            &QAction::toggled,
            [button, buttonAction](bool isChecked)
            {
                button->setChecked(isChecked);
                button->setToolTip(buttonAction->toolTip());
            }
        );
    }
    else
    {
        connect(button, &QPushButton::clicked, buttonAction, &QAction::trigger);
    }
}

void NavigationWidget::updatePlaybackButtonsIcons()
{
    bool playing = m_playButton->isChecked();
    updatePlaybackButtonsTooltips();

    m_stepBackwardButton->setIcon(playing
        ? qnSkin->icon(kBackwadIcon)
        : qnSkin->icon(kStepBackwardIcon));
    m_stepForwardButton->setIcon(playing
        ? qnSkin->icon(kForwardIcon)
        : qnSkin->icon(kStepForwardIcon));
}

void NavigationWidget::updatePlaybackButtonsEnabled()
{
    /*
     * Method logic: enable the following buttons if current widget is:
     * - local image: no buttons
     * - local audio file or exported audio: play/pause, jump forward and backward
     * - local video file or exported video: all buttons
     * - camera: play button and:
     * - - if has footage: jump backward, step backward
     * - - if NOT on the live (has footage by default): jump forward, step forward
     * - I/O module: play button and:
     * - - if has footage: jump backward
     * - - if NOT on the live (has footage by default): jump forward
     */

    /* If not playable, it is a local image, all buttons must be disabled. */
    bool playable = navigator()->isPlayingSupported();

    /* Makes sense only for cameras. Local files are never live. */
    bool forwardable = !navigator()->isLive();

    /* Side effects are Evil! */
    bool isLocalFile = !navigator()->isLiveSupported();

    /* Will return true if there is at least one camera with archive on the scene, even is local file is selected. */
    bool hasCameraFootage = navigator()->hasArchive();

    /* Check if current widget has real video (not image, not audio file, not I/O module). */
    bool hasVideo = navigator()->currentWidgetHasVideo();

    /* All options except local image. */
    m_playButton->setEnabled(playable);

    /* Local/exported video, camera with footage. */
    updateBackwardButtonEnabled();

    /* Local/exported video, camera with footage but not on live. */
    bool canStepForwardOrSpeedUp = playable && ((hasCameraFootage && forwardable) || isLocalFile) && hasVideo;
    m_stepForwardButton->setEnabled(canStepForwardOrSpeedUp);

    /* Local/exported file (not image), camera or I/O module with footage. */
    bool canJumpBackward = playable && (hasCameraFootage || isLocalFile);
    m_jumpBackwardButton->setEnabled(canJumpBackward);

    /* Local/exported file (not image), camera or I/O module with footage but not on live. */
    bool canJumpForward = playable && ((hasCameraFootage && forwardable) || isLocalFile);
    m_jumpForwardButton->setEnabled(canJumpForward);

    /*
     * For now, there is a bug when I/O module is placed on the scene together with camera (synced).
     * If we select camera, we can change speed to -16x, but when we select I/O module,
     * client will be in strange state: speed slider allows only 0x and 1x, but current speed is -16x.
     * So we making the slider enabled for I/O module to do not make the situation even stranger.
     */
    m_speedSlider->setEnabled(playable && navigator()->isTimelineRelevant());
}

void NavigationWidget::updateJumpButtonsTooltips()
{
    bool hasPeriods = navigator()->currentWidgetFlags() & QnWorkbenchNavigator::WidgetSupportsPeriods;

    action(menu::JumpToStartAction)->setText(hasPeriods ? tr("Previous Chunk") : tr("To Start"));
    m_jumpBackwardButton->setToolTip(action(menu::JumpToStartAction)->toolTip());
    action(menu::JumpToEndAction)->setText(hasPeriods ? tr("Next Chunk") : tr("To End"));
    m_jumpForwardButton->setToolTip(action(menu::JumpToEndAction)->toolTip());

    // TODO: #sivanov Remove this once button widget <-> action enabled sync is implemented. OR when
    // we disable actions and not buttons.
    updatePlaybackButtonsEnabled();
}

void NavigationWidget::updatePlayButtonChecked()
{
    m_playButton->setChecked(navigator()->isPlaying());
}

void NavigationWidget::updateNavigatorSpeedFromSpeedSlider()
{
    if (m_updatingSpeedSliderFromNavigator)
        return;

    QScopedValueRollback<bool> guard(m_updatingNavigatorFromSpeedSlider, true);
    navigator()->setSpeed(m_speedSlider->roundedSpeed());
}

void NavigationWidget::updateSpeedSliderSpeedFromNavigator()
{
    if (m_updatingNavigatorFromSpeedSlider)
        return;

    QScopedValueRollback<bool> guard(m_updatingSpeedSliderFromNavigator, true);
    m_speedSlider->setSpeedRange(navigator()->minimalSpeed(), navigator()->maximalSpeed());
    m_speedSlider->setSpeed(navigator()->speed());
    updatePlayButtonChecked();
}

void NavigationWidget::updateSpeedSliderParametersFromNavigator()
{
    qreal minimalSpeed = navigator()->minimalSpeed();
    qreal maximalSpeed = navigator()->maximalSpeed();

    qreal speedStep, defaultSpeed;
    if (m_playButton->isChecked())
    {
        speedStep = 1.0;
        defaultSpeed = 1.0;
    }
    else
    {
        speedStep = 0.25;
        defaultSpeed = 0.0;
        maximalSpeed = qMin(qMax(1.0, maximalSpeed * speedStep), maximalSpeed);
        minimalSpeed = qMax(qMin(-1.0, minimalSpeed * speedStep), minimalSpeed);
    }

    /* The calls that follow may change speed */
    QScopedValueRollback<bool> guard(m_updatingSpeedSliderFromNavigator, true);

    m_speedSlider->setSpeedRange(minimalSpeed, maximalSpeed);
    m_speedSlider->setMinimalSpeedStep(speedStep);
    m_speedSlider->setDefaultSpeed(defaultSpeed);
}

void NavigationWidget::updateBackwardButtonEnabled()
{
    const bool playable = navigator()->isPlayingSupported();
    const bool isLocalFile = !navigator()->isLiveSupported();
    const bool hasCameraFootage = navigator()->hasArchive();
    const bool hasVideo = navigator()->currentWidgetHasVideo();
    const bool canStepBackwardOrSpeedDown = playable && hasVideo
        && (hasCameraFootage || isLocalFile);
    const bool hasVmax = navigator()->hasVmax();
    const bool canStepBackward = canStepBackwardOrSpeedDown
        && (!hasVmax || navigator()->speed() > 1.0);

    m_speedSlider->setRestrictEnable(hasVmax);
    m_stepBackwardButton->setEnabled(canStepBackward);
    updatePlaybackButtonsTooltips();
}

void NavigationWidget::updatePlaybackButtonsTooltips()
{
    const bool playing = m_playButton->isChecked();

    if (!m_stepBackwardButton->isEnabled())
    {
        action(menu::PreviousFrameAction)->setText(m_rewindDisabledText);
        m_stepBackwardButton->setToolTip(m_rewindDisabledText);
    }
    else
    {
        action(menu::PreviousFrameAction)->setText(
            playing ? tr("Speed Down") : tr("Previous Frame"));
        m_stepBackwardButton->setToolTip(action(menu::PreviousFrameAction)->toolTip());
    }

    action(menu::NextFrameAction)->setText(playing ? tr("Speed Up") : tr("Next Frame"));
    m_stepForwardButton->setToolTip(action(menu::NextFrameAction)->toolTip());
}

void NavigationWidget::showMessage(const QString& text)
{
    if (!m_messages.isEmpty())
        return;

    const auto id = SceneBanners::instance()->add(text);
    m_messages.insert(id);

    connect(SceneBanners::instance(), &SceneBanners::removed, this,
        [this](const nx::Uuid& id) { m_messages.remove(id); });
}

void NavigationWidget::at_stepBackwardButton_clicked()
{
    if (!m_stepBackwardButton->isEnabled())
        return;

    if (m_playButton->isChecked())
    {
        m_speedSlider->speedDown();
        if (qFuzzyIsNull(m_speedSlider->speed()))
            m_speedSlider->speedDown(); /* Skip 'pause'. */
    }
    else
    {
        navigator()->stepBackward();
    }
    updateBackwardButtonEnabled();
}

void NavigationWidget::at_stepForwardButton_clicked()
{
    if (!m_stepForwardButton->isEnabled())
        return;

    if (m_playButton->isChecked())
    {
        m_speedSlider->speedUp();
        if (qFuzzyIsNull(m_speedSlider->speed()))
            m_speedSlider->speedUp(); /* Skip 'pause'. */
    }
    else
    {
        navigator()->stepForward();
    }
    updateBackwardButtonEnabled();
}

} // nx::vms::client::desktop::workbench::timeline
