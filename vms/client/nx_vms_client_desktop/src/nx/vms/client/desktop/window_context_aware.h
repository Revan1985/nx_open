// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/vms/client/core/window_context_aware.h>
#include <nx/vms/client/desktop/menu/actions.h>

class QAction;
class QWidget;

class QnWorkbenchContext;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;

namespace nx::vms::client::desktop {

namespace menu { class Manager; }

class MainWindow;
class SystemContext;
class WindowContext;
class Workbench;

class WindowContextAware: public nx::vms::client::core::WindowContextAware
{
    using base_type = nx::vms::client::core::WindowContextAware;

public:
    WindowContextAware(WindowContext* windowContext);
    WindowContextAware(WindowContextAware* windowContextAware);
    virtual ~WindowContextAware();

    WindowContext* windowContext() const;

    /** System Context, which is selected as current in the given window. */
    SystemContext* system() const;

    QnWorkbenchContext* workbenchContext() const;

    Workbench* workbench() const;

    QnWorkbenchDisplay* display() const;

    QnWorkbenchNavigator* navigator() const;

    menu::Manager* menu() const;

    QAction* action(const menu::IDType id) const;

    MainWindow* mainWindow() const;

    /**
     * @return The same as mainWindow() but casted to QWidget*, so caller don't need to include
     * MainWindow header.
     */
    QWidget* mainWindowWidget() const;
};

} // namespace nx::vms::client::desktop
