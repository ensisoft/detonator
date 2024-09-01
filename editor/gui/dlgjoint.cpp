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

    Show();

}
DlgJoint::~DlgJoint()
{

}

void DlgJoint::Show()
{
    SetVisible(mUI.lblNodeAPosition,         false);
    SetVisible(mUI.lblNodeBPosition,         false);
    SetVisible(mUI.lblMinDistance,           false);
    SetVisible(mUI.lblMaxDistance,           false);
    SetVisible(mUI.lblStiffness,             false);
    SetVisible(mUI.lblDamping,               false);
    SetVisible(mUI.lblLowerAngleLimit,       false);
    SetVisible(mUI.lblUpperangleLimit,       false);
    SetVisible(mUI.lblMotorSpeed,            false);
    SetVisible(mUI.lblMotorTorque,           false);
    SetVisible(mUI.lblMotorRotation,         false);
    SetVisible(mUI.lblMotorForce,            false);
    SetVisible(mUI.upperAngle,               false);
    SetVisible(mUI.lowerAngle,               false);
    SetVisible(mUI.motorSpeed,               false);
    SetVisible(mUI.motorTorque,              false);
    SetVisible(mUI.motorRotation,            false);
    SetVisible(mUI.motorForce,               false);
    SetVisible(mUI.chkEnableMotor,           false);
    SetVisible(mUI.chkEnableLimit,           false);
    SetVisible(mUI.srcX,                     false);
    SetVisible(mUI.srcY,                     false);
    SetVisible(mUI.dstX,                     false);
    SetVisible(mUI.dstY,                     false);
    SetVisible(mUI.minDist,                  false);
    SetVisible(mUI.maxDist,                  false);
    SetVisible(mUI.stiffness,                false);
    SetVisible(mUI.damping,                  false);
    SetVisible(mUI.btnResetMinDistance,      false);
    SetVisible(mUI.btnResetMaxDistance,      false);
    SetVisible(mUI.btnResetDstAnchor,        false);
    SetVisible(mUI.btnResetSrcAnchor,        false);
    SetVisible(mUI.lowerTranslationLimit,    false);
    SetVisible(mUI.upperTranslationLimit,    false);
    SetVisible(mUI.lblLowerTranslationLimit, false);
    SetVisible(mUI.lblUpperTranslationLimit, false);
    SetVisible(mUI.lblDirAngle,              false);
    SetVisible(mUI.dirAngle,                 false);

    SetVisible(mUI.jointAnchors,             true);


    SetValue(mUI.cmbType, mJoint.type);
    SetValue(mUI.srcX, mJoint.src_node_anchor_point.x);
    SetValue(mUI.srcY, mJoint.src_node_anchor_point.y);
    SetValue(mUI.dstX, mJoint.dst_node_anchor_point.x);
    SetValue(mUI.dstY, mJoint.dst_node_anchor_point.y);
    SetValue(mUI.chkCollideConnected, mJoint.CollideConnected());

    if (mJoint.type == game::EntityClass::PhysicsJointType::Distance)
    {
        SetVisible(mUI.lblNodeAPosition,    true);
        SetVisible(mUI.lblNodeBPosition,    true);
        SetVisible(mUI.lblMinDistance,      true);
        SetVisible(mUI.lblMaxDistance,      true);
        SetVisible(mUI.lblStiffness,        true);
        SetVisible(mUI.lblDamping,          true);
        SetVisible(mUI.srcX,                true);
        SetVisible(mUI.srcY,                true);
        SetVisible(mUI.dstX,                true);
        SetVisible(mUI.dstY,                true);
        SetVisible(mUI.minDist,             true);
        SetVisible(mUI.maxDist,             true);
        SetVisible(mUI.stiffness,           true);
        SetVisible(mUI.damping,             true);
        SetVisible(mUI.btnResetMinDistance, true);
        SetVisible(mUI.btnResetMaxDistance, true);
        SetVisible(mUI.btnResetDstAnchor,   true);
        SetVisible(mUI.btnResetSrcAnchor,   true);

        const auto& params = std::get<game::EntityClass::DistanceJointParams>(mJoint.params);

        SetValue(mUI.stiffness, params.stiffness);
        SetValue(mUI.damping, params.damping);
        SetValue(mUI.minDist, params.min_distance.value_or(-0.1f));
        SetValue(mUI.maxDist, params.max_distance.value_or(-0.1f));

        SetSuffix(mUI.stiffness, " N/m"); // Newtons per meter
        SetSuffix(mUI.damping, " N⋅s/m"); // Newton seconds per meter
    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Revolute)
    {
        SetVisible(mUI.lblNodeAPosition,    true);
        SetVisible(mUI.srcX,                true);
        SetVisible(mUI.srcY,                true);
        SetVisible(mUI.btnResetSrcAnchor,   true);
        SetVisible(mUI.lblLowerAngleLimit,  true);
        SetVisible(mUI.lblUpperangleLimit,  true);
        SetVisible(mUI.lblMotorSpeed,       true);
        SetVisible(mUI.lblMotorTorque,      true);
        SetVisible(mUI.upperAngle,          true);
        SetVisible(mUI.lowerAngle,          true);
        SetVisible(mUI.motorSpeed,          true);
        SetVisible(mUI.motorTorque,         true);
        SetVisible(mUI.chkEnableLimit,      true);
        SetVisible(mUI.chkEnableMotor,      true);
        SetVisible(mUI.lblMotorRotation,    true);
        SetVisible(mUI.motorRotation,       true);
        SetRange(mUI.motorSpeed, 0.0f, 100.0f);

        const auto& params = std::get<game::EntityClass::RevoluteJointParams>(mJoint.params);
        SetValue(mUI.lowerAngle, params.lower_angle_limit);
        SetValue(mUI.upperAngle, params.upper_angle_limit);
        SetValue(mUI.motorSpeed, std::fabs(params.motor_speed));
        SetValue(mUI.motorTorque, params.motor_torque);
        SetValue(mUI.chkEnableMotor, params.enable_motor);
        SetValue(mUI.chkEnableLimit, params.enable_limit);

        // positive speed is counterclockwise rotation of the joint
        if (params.motor_speed >= 0.0f)
            SetValue(mUI.motorRotation, "Counterclockwise");
        else SetValue(mUI.motorRotation, "Clockwise");

    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Weld)
    {
        SetVisible(mUI.lblNodeAPosition,    true);
        SetVisible(mUI.srcX,                true);
        SetVisible(mUI.srcY,                true);
        SetVisible(mUI.btnResetSrcAnchor,   true);
        SetVisible(mUI.lblStiffness,        true);
        SetVisible(mUI.lblDamping,          true);
        SetVisible(mUI.stiffness,           true);
        SetVisible(mUI.damping,             true);

        const auto& params = std::get<game::EntityClass::WeldJointParams>(mJoint.params);
        SetValue(mUI.damping, params.damping);
        SetValue(mUI.stiffness, params.stiffness);

        SetSuffix(mUI.stiffness, " N⋅m");
        SetSuffix(mUI.damping, " N⋅m/s");
    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Prismatic)
    {
        SetVisible(mUI.lblNodeAPosition,         true);
        SetVisible(mUI.srcX,                     true);
        SetVisible(mUI.srcY,                     true);
        SetVisible(mUI.btnResetSrcAnchor,        true);
        SetVisible(mUI.motorTorque,              true);
        SetVisible(mUI.lblMotorTorque,           true);
        SetVisible(mUI.lblMotorSpeed,            true);
        SetVisible(mUI.motorSpeed,               true);
        SetVisible(mUI.motorTorque,              true);
        SetVisible(mUI.chkEnableLimit,           true);
        SetVisible(mUI.chkEnableMotor,           true);
        SetVisible(mUI.lowerTranslationLimit,    true);
        SetVisible(mUI.upperTranslationLimit,    true);
        SetVisible(mUI.lblUpperTranslationLimit, true);
        SetVisible(mUI.lblLowerTranslationLimit, true);
        SetVisible(mUI.lblDirAngle,              true);
        SetVisible(mUI.dirAngle,                 true);
        SetRange(mUI.motorSpeed, -100.0f, 100.0f);

        const auto& params = std::get<game::EntityClass::PrismaticJointParams>(mJoint.params);
        SetValue(mUI.chkEnableLimit, params.enable_limit);
        SetValue(mUI.chkEnableMotor, params.enable_motor);
        SetValue(mUI.motorSpeed,     params.motor_speed);
        SetValue(mUI.motorTorque,    params.motor_torque);
        SetValue(mUI.lowerTranslationLimit, params.lower_limit);
        SetValue(mUI.upperTranslationLimit, params.upper_limit);
        SetValue(mUI.dirAngle, params.direction_angle);
    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Motor)
    {
        SetVisible(mUI.jointAnchors,             false);
        SetVisible(mUI.lblMotorForce,            true);
        SetVisible(mUI.lblMotorTorque,           true);
        SetVisible(mUI.motorForce,               true);
        SetVisible(mUI.motorTorque,              true);

        const auto& params = std::get<game::EntityClass::MotorJointParams>(mJoint.params);
        SetValue(mUI.motorForce, params.max_force);
        SetValue(mUI.motorTorque, params.max_torque);
    }
}

bool DlgJoint::Apply()
{
    if (!MustHaveInput(mUI.cmbDstNode))
        return false;
    if (!MustHaveInput(mUI.cmbSrcNode))
        return false;

    std::string src_node_id = GetItemId(mUI.cmbSrcNode);
    std::string dst_node_id = GetItemId(mUI.cmbDstNode);
    if (src_node_id == dst_node_id)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setText("The src an dst nodes are the same node.\n"
                    "You can't create a joint that would connect a rigid body to itself.");
        msg.setStandardButtons(QMessageBox::StandardButton::Ok);
        msg.exec();
        mUI.cmbSrcNode->setFocus();
        return false;
    }
    mJoint.name        = GetValue(mUI.jointName);
    mJoint.type        = GetValue(mUI.cmbType);
    mJoint.src_node_id = std::move(src_node_id);
    mJoint.dst_node_id = std::move(dst_node_id);
    mJoint.dst_node_anchor_point.x = GetValue(mUI.dstX);
    mJoint.dst_node_anchor_point.y = GetValue(mUI.dstY);
    mJoint.src_node_anchor_point.x = GetValue(mUI.srcX);
    mJoint.src_node_anchor_point.y = GetValue(mUI.srcY);
    mJoint.SetFlag(game::EntityClass::PhysicsJoint::Flags::CollideConnected, GetValue(mUI.chkCollideConnected));

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
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Revolute)
    {
        const auto& motor_direction = mUI.motorRotation->currentText();

        game::EntityClass::RevoluteJointParams params;
        params.upper_angle_limit = GetValue(mUI.upperAngle);
        params.lower_angle_limit = GetValue(mUI.lowerAngle);
        params.motor_torque = GetValue(mUI.motorTorque);
        params.motor_speed = GetValue(mUI.motorSpeed);
        params.enable_limit = GetValue(mUI.chkEnableLimit);
        params.enable_motor = GetValue(mUI.chkEnableMotor);
        if (motor_direction == "Clockwise")
            params.motor_speed *= -1.0f;
        mJoint.params = params;
    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Weld)
    {
        game::EntityClass::WeldJointParams params;
        params.damping = GetValue(mUI.damping);
        params.stiffness = GetValue(mUI.stiffness);
        mJoint.params = params;
    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Prismatic)
    {
        game::EntityClass::PrismaticJointParams params;
        params.upper_limit = GetValue(mUI.upperTranslationLimit);
        params.lower_limit = GetValue(mUI.lowerTranslationLimit);
        params.motor_torque = GetValue(mUI.motorTorque);
        params.motor_speed = GetValue(mUI.motorSpeed);
        params.enable_limit = GetValue(mUI.chkEnableLimit);
        params.enable_motor = GetValue(mUI.chkEnableMotor);
        params.direction_angle = GetValue(mUI.dirAngle);
        mJoint.params = params;
    }
    else if (mJoint.type == game::EntityClass::PhysicsJointType::Motor)
    {
        game::EntityClass::MotorJointParams params;
        params.max_torque = GetValue(mUI.motorTorque);
        params.max_force = GetValue(mUI.motorForce);
        mJoint.params = params;
    }
    return true;
}

void DlgJoint::on_btnApply_clicked()
{
    Apply();
}

void DlgJoint::on_btnAccept_clicked()
{
    if (Apply())
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

void DlgJoint::on_cmbType_currentIndexChanged(int)
{
    const game::EntityClass::PhysicsJointType type = GetValue(mUI.cmbType);
    if (type == game::EntityClass::PhysicsJointType::Distance)
    {
        mJoint.type = type;
        mJoint.params = game::EntityClass::DistanceJointParams {};
    }
    else if (type == game::EntityClass::PhysicsJointType::Revolute)
    {
        mJoint.type = type;
        mJoint.params = game::EntityClass::RevoluteJointParams {};
    }
    else if (type == game::EntityClass::PhysicsJointType::Weld)
    {
        mJoint.type = type;
        mJoint.params = game::EntityClass::WeldJointParams {};
    }
    else if (type == game::EntityClass::PhysicsJointType::Prismatic)
    {
        mJoint.type = type;
        mJoint.params = game::EntityClass::PrismaticJointParams {};
    }
    else if (type == game::EntityClass::PhysicsJointType::Motor)
    {
        mJoint.type = type;
        mJoint.params = game::EntityClass::MotorJointParams {};
    }

    Show();
}

void DlgJoint::on_srcX_valueChanged(double)
{
    Apply();
}
void DlgJoint::on_srcY_valueChanged(double)
{
    Apply();
}
void DlgJoint::on_dstX_valueChanged(double)
{
    Apply();
}
void DlgJoint::on_dstY_valueChanged(double)
{
    Apply();
}

void DlgJoint::on_dirAngle_valueChanged(double)
{
    Apply();
}

} // namespace
