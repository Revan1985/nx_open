// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "graphics_view.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QWheelEvent>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtQml/QQmlEngine>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneWheelEvent>

#include <client/client_runtime_settings.h>
#include <nx/pathkit/rhi_paint_device.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/trace/trace_categories.h>
#include <nx/vms/client/core/common/helpers/texture_size_helper.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/common/palette.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>

#include "rhi_rendering_item.h"
#include "quick_widget_container.h"

using namespace nx::vms::client::desktop;
using namespace nx::pathkit;

using TextureSizeHelper = nx::vms::client::core::TextureSizeHelper;

struct QnGraphicsView::Private
{
    Private()
    {
    }

    QOpenGLWidget* openGLWidget = nullptr;
    QQuickWidget* quickWidget = nullptr;
    QPointer<RhiRenderingItem> renderingItem;
    QPointer<TextureSizeHelper> textureSizeHelperItem;
};

QnGraphicsView::QnGraphicsView(QGraphicsScene* scene, nx::vms::client::desktop::MainWindow* parent):
    base_type(scene, parent),
    d(new Private())
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    setPaletteColor(this, QPalette::Base, nx::vms::client::core::colorTheme()->color("dark1"));

    const auto graphicsApi = appContext()->runtimeSettings()->graphicsApi();

    if (graphicsApi == GraphicsApi::legacyopengl)
    {
        d->openGLWidget = new QOpenGLWidget(this);
        d->openGLWidget->makeCurrent();
        d->openGLWidget->setAttribute(Qt::WA_Hover);
        setViewport(d->openGLWidget);
        return;
    }

    if (graphicsApi == GraphicsApi::software)
    {
        const auto viewport = new QWidget(this);
        viewport->setAttribute(Qt::WA_Hover);
        setViewport(viewport);
        return;
    }

    auto container = new QuickWidgetContainer();
    container->setAttribute(Qt::WA_Hover);
    setViewport(container);
    container->setAutoFillBackground(false);

    d->quickWidget = new QQuickWidget(
        nx::vms::client::core::appContext()->qmlEngine(), this->viewport());

    d->quickWidget->setObjectName("Scene");

    // QQuickWidget is used only for RHI-based rendering.
    d->quickWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    d->quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    d->quickWidget->setSource(QUrl("Nx/MainScene/Scene.qml"));

    container->setQuickWidget(d->quickWidget);
    container->setView(this);

    if (auto root = d->quickWidget->rootObject())
    {
        d->renderingItem = root->findChild<RhiRenderingItem*>("renderingItem");
        d->textureSizeHelperItem = root->findChild<TextureSizeHelper*>("textureSizeHelper");
        if (d->textureSizeHelperItem)
        {
            // Workaround for QnVideoStreamDisplay and QnGLRenderer using GL_MAX_TEXTURE_INTEGER.
            // Provide new max texture size when
            connect(d->textureSizeHelperItem, &TextureSizeHelper::maxTextureSizeChanged, this,
                [this]()
                {
                    QnGlFunctions::overrideMaxTextureSize(d->textureSizeHelperItem->maxTextureSize());
                });
        }
    }

    NX_ASSERT(d->renderingItem);

    d->quickWidget->quickWindow()->setColor(
        nx::vms::client::core::colorTheme()->color("dark3")); //< window

    connect(d->quickWidget->quickWindow(), &QQuickWindow::beforeFrameBegin, this,
        []()
        {
            TRACE_EVENT_BEGIN("rendering", "QnGraphicsView::frame");
        },
        Qt::DirectConnection);

    connect(d->quickWidget->quickWindow(), &QQuickWindow::afterFrameEnd, this,
        []()
        {
            TRACE_EVENT_END("rendering");
        },
        Qt::DirectConnection);
}

QnGraphicsView::~QnGraphicsView()
{
}

void QnGraphicsView::paintEvent(QPaintEvent* event)
{
    TRACE_EVENT("rendering", "QnGraphicsView::paintEvent");

    // Always make QOpenGLWidget context to be the current context.
    // This is what item paint functions expect and doing otherwise
    // may lead to unpredictable behavior.
    if (d->openGLWidget)
    {
        d->openGLWidget->makeCurrent();
        NX_ASSERT(QOpenGLContext::currentContext());
        base_type::paintEvent(event);
        return;
    }

    if (!d->quickWidget)
    {
        base_type::paintEvent(event);
        return;
    }

    paintRhi();
}

void QnGraphicsView::paintRhi()
{
    // It would be better to skip rendering when previous frame has not been rendered yet by
    // QQuickWidget but that breaks paint tracking for resource widgets in QnWorkbenchRenderWatcher.

    if (!d->renderingItem)
        return;

    auto paintDevice = d->renderingItem->paintDevice();
    const auto pixelRatio = d->quickWidget->quickWindow()->effectiveDevicePixelRatio();

    paintDevice->setSize(d->quickWidget->quickWindow()->size() * pixelRatio);
    paintDevice->setDevicePixelRatio(pixelRatio);
    paintDevice->clear();

    QPainter painter(paintDevice);
    render(&painter, viewport()->rect());

    // renderingItem->update() won't work because renderingItem rendering is triggered by
    // QQuickWindow's beforeRendering() and beforeRenderPassRecording() signals.
    d->quickWidget->quickWindow()->update();
}

void QnGraphicsView::wheelEvent(QWheelEvent* event)
{
    // Copied from QGraphicsView implementation. The only difference is QAbstractScrollArea
    // implementation is not invoked.

    if (!scene() || !isInteractive())
        return;

    event->ignore();

    QGraphicsSceneWheelEvent wheelEvent(QEvent::GraphicsSceneWheel);
    wheelEvent.setWidget(viewport());
    wheelEvent.setScenePos(mapToScene(event->position().toPoint()));
    wheelEvent.setScreenPos(event->globalPosition().toPoint());
    wheelEvent.setButtons(event->buttons());
    wheelEvent.setModifiers(event->modifiers());
    const auto angleDelta = event->angleDelta();
    if (angleDelta.y() != 0)
    {
        wheelEvent.setDelta(angleDelta.y());
        wheelEvent.setOrientation(Qt::Vertical);
    }
    else
    {
        wheelEvent.setDelta(angleDelta.x());
        wheelEvent.setOrientation(Qt::Horizontal);
    }
    wheelEvent.setPhase(event->phase());
    wheelEvent.setInverted(event->isInverted());
    wheelEvent.setAccepted(false);
    QApplication::sendEvent(scene(), &wheelEvent);
    event->setAccepted(wheelEvent.isAccepted());
}

void QnGraphicsView::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
        setBackgroundBrush(palette().base());
}

QQuickWidget* QnGraphicsView::quickWidget() const
{
    return d->quickWidget;
}
