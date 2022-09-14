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

#include "config.h"

#include "editor/gui/dlgjoint.h"
#include "editor/gui/utility.h"

namespace gui
{
DlgJoint::DlgJoint(QWidget* parent, const game::EntityClass& klass, game::EntityClass::PhysicsJoint& joint)
  : QDialog(parent)
  , mEntity(klass)
  , mJoint(joint)
{
    mUI.setupUi(this);

    PopulateFromEnum<game::EntityClass::PhysicsJointType>(mUI.cmbType);

    std::vector<ResourceListItem> nodes_with_rigid_bodies;
    for (size_t i=0; i<mEntity.GetNumNodes(); ++i)
    {
        const auto& node = mEntity.GetNode(i);
        if (!node.HasRigidBody())
            continue;
        ResourceListItem item;
        item.name = app::FromUtf8(node.GetName());
        item.id   = app::FromUtf8(node.GetId());
        nodes_with_rigid_bodies.push_back(std::move(item));
    }
    SetValue(mUI.jointID, mJoint.id);
    SetValue(mUI.jointName, mJoint.name);
    SetList(mUI.cmbSrcNode, nodes_with_rigid_bodies);
    SetList(mUI.cmbDstNode, nodes_with_rigid_bodies);
    SetValue(mUI.cmbSrcNode, ListItemId(mJoint.src_node_id));
    SetValue(mUI.cmbDstNode, ListItemId(mJoint.dst_node_id));
    SetValue(mUI.srcX, mJoint.src_node_anchor_point.x);
    SetValue(mUI.srcY, mJoint.src_node_anchor_point.y);
    SetValue(mUI.dstX, mJoint.dst_node_anchor_point.x);
    SetValue(mUI.dstY, mJoint.dst_node_anchor_point.y);
    if (mJoint.type == game::EntityClass::PhysicsJointType::Distance)
    {
        const auto& params = std::get<game::EntityClass::DistanceJointParams>(mJoint.params);
        SetValue(mUI.stiffness, params.stiffness);
        SetValue(mUI.damping, params.damping);
        SetValue(mUI.minDist, params.min_distance.value_or(-0.1f));
        SetValue(mUI.maxDist, params.max_distance.value_or(-0.1f));
    }
}
DlgJoint::~DlgJoint()
{

}

void DlgJoint::on_btnAccept_clicked()
{
    if (!MustHaveInput(mUI.cmbDstNode))
        return;
    if (!MustHaveInput(mUI.cmbSrcNode))
        return;

    mJoint.name        = GetValue(mUI.jointName);
    mJoint.type        = GetValue(mUI.cmbType);
    mJoint.src_node_id = GetItemId(mUI.cmbSrcNode);
    mJoint.dst_node_id = GetItemId(mUI.cmbDstNode);
    mJoint.dst_node_anchor_point.x = GetValue(mUI.dstX);
    mJoint.dst_node_anchor_point.y = GetValue(mUI.dstY);
    mJoint.src_node_anchor_point.x = GetValue(mUI.srcX);
    mJoint.src_node_anchor_point.y = GetValue(mUI.srcY);
    if (mJoint.type == game::EntityClass::PhysicsJointType::Distance)
    {
        game::EntityClass::DistanceJointParams params;
        params.stiffness = GetValue(mUI.stiffness);
        params.damping   = GetValue(mUI.damping);
        const float min_dist = GetValue(mUI.minDist);
        const float max_dist = GetValue(mUI.maxDist);
        if (min_dist != -0.1f)
            params.min_distance = min_dist;
        if (max_dist != -0.1f)
            params.max_distance = max_dist;
        mJoint.params = params;
    }

    accept();

}
void DlgJoint::on_btnCancel_clicked()
{
    reject();
}

void DlgJoint::on_btnResetSrcAnchor_clicked()
{
    SetValue(mUI.srcX, 0.0f);
    SetValue(mUI.srcY, 0.0f);
}
void DlgJoint::on_btnResetDstAnchor_clicked()
{
    SetValue(mUI.dstX, 0.0f);
    SetValue(mUI.dstY, 0.0f);
}

void DlgJoint::on_btnResetMinDistance_clicked()
{
    SetValue(mUI.minDist, -0.1f);
}
void DlgJoint::on_btnResetMaxDistance_clicked()
{
    SetValue(mUI.maxDist, -0.1f);
}


} // namespace
