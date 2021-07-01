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

#define LOGTAG "gui"

#include "config.h"

#include "base/math.h"

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

    SetValue(mUI.chkEntityLifetime, Qt::PartiallyChecked);
    SetValue(mUI.chkKillAtLifetime, Qt::PartiallyChecked);
    SetValue(mUI.chkUpdateEntity, Qt::PartiallyChecked);
    SetValue(mUI.chkTickEntity, Qt::PartiallyChecked);
    SetValue(mUI.chkKeyEvents, Qt::PartiallyChecked);

    std::vector<ListItem> tracks;
    for (size_t i=0; i<klass.GetNumTracks(); ++i)
    {
        const auto& track = klass.GetAnimationTrack(i);
        ListItem item;
        item.name = app::FromUtf8(track.GetName());
        item.id   = app::FromUtf8(track.GetId());
        tracks.push_back(std::move(item));
    }
    SetList(mUI.idleAnimation, tracks);
    SetValue(mUI.idleAnimation, ListItemId(node.GetIdleAnimationId()));
    if (mNodeClass.HasLifetimeSetting())
        SetValue(mUI.entityLifetime, mNodeClass.GetLifetime());

    GetFlag(game::SceneNodeClass::Flags::LimitLifetime,  mUI.chkEntityLifetime);
    GetFlag(game::SceneNodeClass::Flags::KillAtLifetime, mUI.chkKillAtLifetime);
    GetFlag(game::SceneNodeClass::Flags::UpdateEntity,   mUI.chkUpdateEntity);
    GetFlag(game::SceneNodeClass::Flags::TickEntity,     mUI.chkTickEntity);
    GetFlag(game::SceneNodeClass::Flags::WantsKeyEvents, mUI.chkKeyEvents);
}

void DlgEntity::on_btnAccept_clicked()
{
    mNodeClass.SetIdleAnimationId(GetItemId(mUI.idleAnimation));
    mNodeClass.ResetLifetime();
    const float lifetime = GetValue(mUI.entityLifetime);
    if (lifetime != -1.0)
        mNodeClass.SetLifetime(lifetime);

    SetFlag(game::SceneNodeClass::Flags::LimitLifetime,  mUI.chkEntityLifetime);
    SetFlag(game::SceneNodeClass::Flags::KillAtLifetime, mUI.chkKillAtLifetime);
    SetFlag(game::SceneNodeClass::Flags::UpdateEntity,   mUI.chkUpdateEntity);
    SetFlag(game::SceneNodeClass::Flags::TickEntity,     mUI.chkTickEntity);
    SetFlag(game::SceneNodeClass::Flags::WantsKeyEvents, mUI.chkKeyEvents);

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

void DlgEntity::on_btnResetLifetime_clicked()
{
    SetValue(mUI.entityLifetime, -1.0);
}

void DlgEntity::on_entityLifetime_valueChanged(double value)
{
    // QDoubleSpinBox doesn't unfortunately have a feature for detecting
    // "no value has been set", but it has a "special value text" which is
    // display when the value equals the minimum. we're abusing -1.0 here
    // as a special value for indicating "no value has been set". Thus if
    // the spin value is changed the "real" value must be clamped between
    // 0.0f and X to hide the -1.0 special.
    value = math::clamp(0.0, mUI.entityLifetime->maximum(), value);
    SetValue(mUI.entityLifetime, value);
}

void DlgEntity::SetFlag(game::SceneNodeClass::Flags flag, QCheckBox* chk)
{
    if (chk->checkState() == Qt::PartiallyChecked)
        mNodeClass.ClearFlagSetting(flag);
    else mNodeClass.SetFlag(flag, GetValue(chk));
}

void DlgEntity::GetFlag(game::SceneNodeClass::Flags flag, QCheckBox* chk)
{
    SetValue(chk, Qt::PartiallyChecked);
    if (mNodeClass.HasFlagSetting(flag))
        SetValue(chk, mNodeClass.TestFlag(flag));
}

} // namespace
