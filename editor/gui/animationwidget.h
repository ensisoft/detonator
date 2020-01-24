// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_animationwidget.h"
#include "warnpop.h"

#include "editor/gui/mainwidget.h"
#include "scene/animation.h"

namespace gfx {
    class Painter;
} // gfx

namespace app {
    class Resource;
} // app

namespace gui
{
    class AnimationWidget : public MainWidget
    {
        Q_OBJECT
    public:
        AnimationWidget();
        AnimationWidget(const app::Resource& resource, app::Workspace* workspace);
       ~AnimationWidget();

        virtual void addActions(QToolBar& bar) override;
        virtual void addActions(QMenu& menu) override;
        virtual void setWorkspace(app::Workspace* workspace) override;

    private slots:
        void on_actionNewRect_triggered();
        void on_actionDeleteComponent_triggered();
        void on_components_customContextMenuRequested(QPoint);

    private:
        void paintScene(gfx::Painter& painter, double secs);
    private:
        class ComponentModel;
        class Tool;
        class PlaceTool;
        class Fuck;

    private:
        Ui::AnimationWidget mUI;
    private:
        app::Workspace* mWorkspace = nullptr;
        std::unique_ptr<Tool> mCurrentTool;
        std::unique_ptr<ComponentModel> mModel;
    private:
        scene::Animation mAnimation;

    };
} // namespace
