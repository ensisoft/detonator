// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QtWidgets>
#include "warnpop.h"

namespace gui
{
    class Settings;
    class Clipboard;

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

        // Different actions that the widget may or may not support
        // in its current state.
        enum class Actions {
            CanZoomIn  = 0x1,
            CanZoomOut = 0x2,
            CanCut     = 0x4,
            CanCopy    = 0x8,
            CanPaste   = 0x10,
            CanUndo    = 0x20,
            CanReloadTextures = 0x40,
            CanReloadShaders  = 0x80
        };

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

        // Check whether the widget can take some action in the current state.
        // Returns true if the action can be taken or false to indicate that the
        // action is currently not possible.
        // The MainWindow will call this at different times for different actions.
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard = nullptr) const
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

        // Cut the currently selected object and place into the clipboard.
        virtual void Cut(Clipboard& clipboard)
        {}

        // Copy the currently selected object and place into the clipboard.
        virtual void Copy(Clipboard& clipboard) const
        {}

        // Paste the current object from the clipboard into the widget.
        virtual void Paste(const Clipboard& clipboard)
        {}

        // Undo the last action on the undo stack.
        virtual void Undo()
        {}
        
        // Returns true if the widget has whatever are considered
        // as "unsaved changes". 
        virtual bool HasUnsavedChanges() const
        { return false; }

        // Request the widget to save it's content. After this
        // HasUnsavedChanges should no longer be true (if it was true before)
        virtual void Save()
        {}

        // Returns true if the widget wants to close itself. The caller should
        // not call "ConfirmClose" or anything else on the widget other than shutdown
        // since the widget has made the decision to want to close already.
        virtual bool ShouldClose() const
        { return false; }

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

        void SetId(QString id)
        { mId = id;}
        QString GetId() const
        { return mId; }

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
        // Emitted when the widget's state regarding possible actions that
        // can take place has changed. The MainWindow will then query the
        // widget again to see which actions (cut,copy, paste, zoom etc)
        // can or cannot take place.
        void ActionStateChanged();
    private:
        QString mId;
    };

} // namespace
