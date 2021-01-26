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

#define LOGTAG "entity"

#include "config.h"

#include "editor/gui/dlgentity.h"
#include "editor/gui/utility.h"
#include "editor/app/utility.h"

namespace gui
{
DlgEntity::DlgEntity(QWidget* parent, const game::EntityClass& klass, game::SceneNodeClass& node)
    : QDialog(parent)
    , mEntityClass(klass)
    , mNodeClass(node)
{
    mUI.setupUi(this);

    int current_index = -1;
    QSignalBlocker blocker(mUI.idleAnimation);
    for (size_t i=0; i<klass.GetNumTracks(); ++i)
    {
        const auto& track = klass.GetAnimationTrack(i);
        const auto& name  = app::FromUtf8(track.GetName());
        const auto& id    = app::FromUtf8(track.GetId());
        mUI.idleAnimation->addItem(name, QVariant(id));
        if (track.GetId() == node.GetIdleAnimationId())
            current_index = static_cast<int>(i);
    }
    if (current_index == -1)
    {
        SetValue(mUI.idleAnimation, -1);
    }
    else
    {
        SetValue(mUI.idleAnimation, current_index);
    }
}

 void DlgEntity::on_btnAccept_clicked()
 {
    if (mUI.idleAnimation->currentIndex() == -1)
    {
        mNodeClass.SetIdleAnimationId("");
    }
    else
    {
        const auto& id = mUI.idleAnimation->currentData().toString();
        mNodeClass.SetIdleAnimationId(app::ToUtf8(id));
    }
    accept();
 }
 void DlgEntity::on_btnCancel_clicked()
 {
    reject();
 }

 void DlgEntity::on_btnResetIdleAnimation_clicked()
 {
    SetValue(mUI.idleAnimation, -1);
 }

} // namespace
