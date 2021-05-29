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
