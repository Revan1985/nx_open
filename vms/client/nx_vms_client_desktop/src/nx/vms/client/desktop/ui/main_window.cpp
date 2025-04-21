// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlError>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/event_search/models/visible_item_data_decorator_model.h>
#include <nx/vms/client/desktop/ui/image_providers/resource_icon_provider.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/opengl/gl_functions.h>

namespace {

// The class is used to access protected fields of QMouseEvent.
class MouseEventWorkaround: public QMouseEvent
{
public:
    void setDoubleClick(bool value)
    {
        m_doubleClick = value;
    }
};

} // namespace

namespace nx::vms::client::desktop {
namespace ui {
namespace experimental {

class MainWindow::Private: public QObject
{
    MainWindow* const q = nullptr;

public:
    Private(MainWindow* parent);

public:
    QQuickWidget* sceneWidget = nullptr;
};

MainWindow::Private::Private(MainWindow* parent):
    q(parent)
{
}

//-------------------------------------------------------------------------------------------------

class QuickWidget: public QQuickWidget
{
public:
    using QQuickWidget::QQuickWidget; //< Forward constructors.

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        // QTBUG-77393 fix: in QML second press event of a doubleclick does not have
        // Qt::MouseEventCreatedDoubleClick flag set.

        QMouseEvent pressEvent(QEvent::MouseButtonPress,
            event->position(), event->scenePosition(), event->globalPosition(),
            event->button(), event->buttons(), event->modifiers(), event->pointingDevice());

        static_cast<MouseEventWorkaround*>(&pressEvent)->setDoubleClick(true);

        QCoreApplication::sendEvent(quickWindow(), &pressEvent);
        event->setAccepted(pressEvent.isAccepted());

        QMouseEvent mappedEvent(event->type(),
            event->position(), event->scenePosition(), event->globalPosition(),
            event->button(), event->buttons(), event->modifiers(), event->pointingDevice());

        QCoreApplication::sendEvent(quickWindow(), &mappedEvent);
    }
};

//-------------------------------------------------------------------------------------------------

MainWindow::MainWindow(QQmlEngine* engine, WindowContext* context, QWidget* parent):
    base_type(parent),
    WindowContextAware(context),
    d(new Private(this))
{
    engine->addImageProvider("resource", new ResourceIconProvider());
    engine->addImageProvider("previews",
        new core::VisibleItemDataDecoratorModel::PreviewProvider());

    d->sceneWidget = new QuickWidget(engine, this);

    d->sceneWidget->rootContext()->setContextProperty(lit("workbench"), workbench());
    d->sceneWidget->rootContext()->setContextProperty(WindowContext::kQmlPropertyName, context);

    d->sceneWidget->setSource(lit("main.qml"));
    d->sceneWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    if (d->sceneWidget->status() == QQuickWidget::Error)
    {
        for (const auto& error: d->sceneWidget->errors())
            NX_ERROR(this, error.toString());
    }

    d->sceneWidget->show();
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    d->sceneWidget->resize(event->size());
}

} // namespace experimental
} // namespace ui
} // namespace nx::vms::client::desktop
