
#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QtWidgets>
#include "warnpop.h"

namespace gui
{
    class Settings;

    // MainWidget interface is an abstraction for extending the
    // application functionality vertically starting from
    // a visible user interface. The MainWidgets plug into the
    // MainWindow and are then subsequently managed by the MainWindow
    // which can display/hide/activate etc. them.
    class MainWidget : public QWidget
    {
        Q_OBJECT

    public:
        virtual ~MainWidget() = default;

        // Returns whether the widget does accelerated rendering and needs to
        // run in an accelerated "game style" loop.
        virtual bool IsAccelerated() const
        { return true; }

        // Load the widget and the underlying resource state.
        // This is invoked when then the application restores
        // windows/widgets that were open the last time application
        // was closed.
        // Should return true if everything was successfully loaded
        // otherwise false. Errors/problems should be logged.
        virtual bool LoadState(const Settings& settings)
        { return true; }

        // Save the widget and the underlying resource state.
        // This is invoked when the application exits and the
        // current application state is being saved for the
        // next application run. (See LoadState).
        // Should return true if everything was successful,
        // otherwise false on error(s).
        virtual bool SaveState(Settings& settings) const
        { return true; }

        // Add user actions specific to the widget in the menu.
        virtual void AddActions(QMenu& menu)
        {}

        // Add user actions specific to the widget in the toolbar.
        virtual void AddActions(QToolBar& bar)
        {}

        // Refresh the widget contents.
        // the MainWindow will call this periodically so the widget
        // can do whatever latency insensitive state updates.
        virtual void Refresh()
        {}

        // Update the widget and the associated animation, simulation
        // etc. data model.
        // This is called at a high frequency that is specified in the
        // workspace settings. The variable dt is the current time step
        // i.e. delta time that can be used in simulations for numerical
        // integration.
        virtual void Update(double dt)
        {}

        // Render the contents of the widget. If the widget doesn't have
        // custom accelerated rendering then likely nothing needs to be done.
        // Otherwise a new frame should be rendered in the accelerated
        // graphics widget. This is called frequently as part of the application's
        // main update/render loop.
        virtual void Render()
        {}

        // Called whenever the widget is getting activated i.e. displayed
        // in the MainWindow.
        virtual void Activate() {}

        // Called when the widget is deactivated and is no longer the
        // active tab in the MainWindow.
        virtual void Deactivate() {}

        // Called when the widget is being closed and deleted.
        // This can happen when the user has requested to simply close
        // the widget/window or when the application is about to exit.
        // If your widget uses any graphics resources (for example
        // it has embedded GfxWidget) it's important to implement this
        // function and dispose all such resources. After the call
        // returns the widget will be deleted.
        virtual void Shutdown() {}

        // Should return true if widget supports zoom in. It's possible
        // that widget doesn't support zoom at all or has already reached
        // the maximum zoom.
        // See ZoomIn and ZoomOut
        virtual bool CanZoomIn() const
        { return false; }

        // Should return true if widget supports zoom in. It's possible
        // that widget doesn't support zoom at all or has already reached
        // the maximum zoom.
        // See ZoomIn and ZoomOut
        virtual bool CanZoomOut() const
        { return false; }

        // Ask the widget to zoom in on the content.
        virtual void ZoomIn() {};

        // Ask the widget to zoom out on the content.
        virtual void ZoomOut() {}

        // If the widget uses graphics resources (shaders) ask
        // them to be reloaded.
        virtual void ReloadShaders() {};
        // If the widget uses graphics resources (textures) ask
        // them to be reloaded.
        virtual void ReloadTextures() {};

        // Called before the widget is being closed by the user.
        // If there are any pending/unsaved changes the widget
        // implementation can ask the user for a confirmation and
        // then proceed to save/discard such changes.
        // If true is returned main window will proceed to close the
        // widget. Otherwise the window is *not* closed and nothing
        // happens.
        virtual bool ConfirmClose() // const
        { return true; }

        struct Stats {
            double time = 0.0;
            float  fps  = 0.0f;
            bool vsync  = false;
        };
        virtual bool GetStats(Stats* stats) const
        { return false; }

    signals:
        // Request to open the given script file in an external script editor.
        void OpenExternalScript(const QString& file);
        // Request to open the given image file in an external image editor.
        void OpenExternalImage(const QString& file);
        // Request to open the given shader file in an external text/shader editor.
        void OpenExternalShader(const QString& file);
        // Request to display the new widget. Will transfer
        // ownership of the widget the mainwindow.
        void OpenNewWidget(MainWidget* widget);

    private:
    };

} // namespace
