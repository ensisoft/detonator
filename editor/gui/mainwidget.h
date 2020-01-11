
#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QtWidgets>
#include "warnpop.h"

namespace gui
{
    class Settings;

    // MainWidget interfact is an abstraction for extending the 
    // application functionality vertically starting from
    // a visible user interface. The MainWidget's plug into
    // MainWindow and are then subsequently managed by the MainWindow
    // which can display/hide/activate etc. them.
    class MainWidget : public QWidget
    {
        Q_OBJECT

    public:
        virtual ~MainWidget() = default;

        // Load the widget user interface state.
        // Returns true if succesful otherwise false to indicate
        // that not all of the could be loaded.
        virtual bool loadState(const Settings& settings)
        { return true; }

        // Save the widget user interface state. 
        virtual bool saveState(Settings& settings) const
        { return true; }

        // Add user actions specific to the widget in the menu.
        virtual void addActions(QMenu& menu)
        {}

        // Add user actions specific to the widget in the toolbar.
        virtual void addActions(QToolBar& bar)
        {}

        // Refresh the widget contents.
        // the MainWindow will call this periodically so the widget
        // can do whatever latency insensitive state updates.
        virtual void refresh() 
        {}

        // Called whenever the widget is getting activated i.e. displayed
        // in the MainWindow.
        virtual void activate() {}

        // Called when the widget is deactivated and is no longer the
        // active tab in the MainWindow.
        virtual void deactivate() {}

        // Called before the application shutsdown.
        // The widget should release it's resources at this point.
        virtual void shutdown() {}

        // Called when the application is starting up. 
        virtual void startup() {}

        virtual void zoomIn() {};
        virtual void zoomOut() {}
    private:
    };

} // namespace
