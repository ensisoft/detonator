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

#include "warnpush.h"
#  include <QPoint>
#  include <QMouseEvent>
#  include <QMessageBox>
#  include <QFile>
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QInputDialog>
#  include <QTextStream>
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#  include <glm/gtx/quaternion.hpp>
#include "warnpop.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>

#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "base/utility.h"
#include "data/json.h"
#include "game/treeop.h"
#include "game/entity_class.h"
#include "game/entity_node_linear_mover.h"
#include "game/entity_node_spline_mover.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "game/entity_node_spatial_node.h"
#include "game/entity_node_fixture.h"
#include "game/entity_node_tilemap_node.h"
#include "game/entity_node_light.h"
#include "game/entity_node_mesh_effect.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "graphics/polygon_mesh.h"
#include "graphics/simple_shape.h"
#include "graphics/text_material.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/app/resource-uri.h"
#include "editor/gui/settings.h"
#include "editor/gui/treewidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/tool.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgparticle.h"
#include "editor/gui/dlgscriptvarname.h"
#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/dlgfont.h"
#include "editor/gui/dlgmaterialparams.h"
#include "editor/gui/dlgjoint.h"
#include "editor/gui/dlganimator.h"
#include "editor/gui/clipboard.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/translation.h"

namespace gui
{

bool CheckEntityNodeNameAvailability(const game::EntityClass& entity, const std::string& name)
{
    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (node.GetName() == name)
            return false;
    }
    return true;
}

std::string GenerateEntityNodeName(const game::EntityClass& entity, const std::string& prefix)
{
    std::string name;
    for (size_t i=0; i<666666; ++i)
    {
        name = prefix + std::to_string(i);
        if (CheckEntityNodeNameAvailability(entity, name))
            break;
    }
    return name;
}

class EntityWidget::SplineModel : public QAbstractTableModel
{
public:
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    }

    bool setData(const QModelIndex& index, const QVariant& variant, int role) override
    {
        const auto row = static_cast<size_t>(index.row());
        const auto col = static_cast<size_t>(index.column());

        bool success = false;
        const float value = variant.toFloat(&success);
        if (!success)
            return false;

        auto point = mSpline->GetPoint(row);
        auto position = point.GetPosition();
        if (col == 0)
            position.x = value;
        else if (col == 1)
            position.y = value;

        point.SetPosition(position);

        mSpline->SetPoint(point, row);

        emit dataChanged(this->index(row, 0), this->index(row, 4));
        return true;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto row = static_cast<size_t>(index.row());
        const auto col = static_cast<size_t>(index.column());
        if (role == Qt::DisplayRole)
        {
            const auto& point = mSpline->GetPoint(row).GetPosition();
            if (col == 0)
                return QString::number(point.x, 'f', 2);
            else if (col == 1)
                return QString::number(point.y, 'f', 2);
        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0)
                return "x";
            else if (section == 1)
                return "y";
        }
        return {};
    }
    int rowCount(const QModelIndex&) const override
    {
        if (mSpline)
            return static_cast<int>(mSpline->GetPointCount());

        return 0;
    }
    int columnCount(const QModelIndex&) const override
    {
        return 2;
    }

    void UpdatePoint(const game::SplinePoint& point, size_t index)
    {
        const auto row = static_cast<int>(index);

        mSpline->SetPoint(point, index);

        emit dataChanged(this->index(row, 0), this->index(row, 4));
    }

    void AppendPoint(const game::SplinePoint& point)
    {
        const auto first_index = mSpline->GetPointCount();
        const auto last_index = first_index + 1 - 1;

        beginInsertRows(QModelIndex(), static_cast<int>(first_index), static_cast<int>(last_index));
        mSpline->AppendPoint(point);
        endInsertRows();
    }
    void PrependPoint(const game::SplinePoint& point)
    {
        const auto first_index = 0;
        const auto last_index = 0;

        beginInsertRows(QModelIndex(), static_cast<int>(first_index), static_cast<int>(last_index));
        mSpline->PrependPoint(point);
        endInsertRows();
    }

    void ErasePoint(size_t index)
    {
        beginRemoveRows(QModelIndex(), static_cast<int>(index), static_cast<int>(index));
        mSpline->ErasePoint(index);
        endRemoveRows();
    }

    void Reset(game::SplineMoverClass* spline = nullptr)
    {
        if (spline == mSpline)
            return;

        beginResetModel();
        mSpline = spline;
        endResetModel();
    }
private:
    game::SplineMoverClass* mSpline = nullptr;
};


class EntityWidget::JointModel : public QAbstractTableModel
{
public:
    explicit JointModel(EntityWidget::State& state) : mState(state)
    {}
    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& joint = mState.entity->GetJoint(index.row());
        const auto* src   = mState.entity->FindNodeById(joint.src_node_id);
        const auto* dst   = mState.entity->FindNodeById(joint.dst_node_id);
        if (role == Qt::DisplayRole)
        {
            switch (index.column()) {
                case 0: return app::toString(joint.type);
                case 1: return app::FromUtf8(joint.name);
                case 2: return app::FromUtf8(src ? src->GetName() : "???");
                case 3: return app::FromUtf8(dst ? dst->GetName() : "???");
                default: BUG("Unknown script variable data index.");
            }
        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            switch (section) {
                case 0: return "Type";
                case 1: return "Name";
                case 2: return "Node";
                case 3: return "Node";
                default: BUG("Unknown script variable data index.");
            }
        }
        return {};
    }
    int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.entity->GetNumJoints());
    }
    int columnCount(const QModelIndex&) const override
    {
        return 4;
    }

    void AddJoint(game::EntityClass::PhysicsJoint&& joint)
    {
        const auto count = static_cast<int>(mState.entity->GetNumJoints());
        beginInsertRows(QModelIndex(), count, count);
        mState.entity->AddJoint(std::move(joint));
        endInsertRows();
    }
    void EditJoint(size_t row, game::EntityClass::PhysicsJoint&& joint)
    {
        mState.entity->SetJoint(row, std::move(joint));
        emit dataChanged(index(row, 0), index(row, 4));
    }
    void UpdateJoint(size_t row)
    {
        emit dataChanged(index(row, 0), index(row, 4));
    }

    void DeleteJoint(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mState.entity->DeleteJoint(row);
        endRemoveRows();
    }

    void Reset()
    {
        beginResetModel();
        endResetModel();
    }
private:
    EntityWidget::State& mState;
};

class EntityWidget::ScriptVarModel : public QAbstractTableModel
{
public:
    explicit ScriptVarModel(EntityWidget::State& state) : mState(state)
    {}
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (index.column() == 0)
            return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;

        const auto& var = mState.entity->GetScriptVar(index.row());
        if (var.IsArray())
            return Qt::ItemIsEnabled;

        const auto type = var.GetType();
        if (type == game::ScriptVar::Type::Integer ||
            type == game::ScriptVar::Type::String ||
            type == game::ScriptVar::Type::Float ||
            type == game::ScriptVar::Type::Boolean)
            return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;

        return Qt::ItemIsEnabled;
    }
    bool setData(const QModelIndex& index, const QVariant& variant, int role) override
    {
        const auto row = index.row();
        const auto col = index.column();

        auto& var = mState.entity->GetScriptVar(index.row());

        bool success = false;
        if (col == 0)
        {
            const auto& name = variant.toString();
            if (name.isEmpty() || name.isNull())
                return false;
            var.SetName(app::ToUtf8(name));
        }
        else if (col == 1)
        {
            if (var.IsArray())
                return false;
            const auto type = var.GetType();
            if (type == game::ScriptVar::Type::Integer)
            {
                const auto val = variant.toInt(&success);
                if (!success)
                    return false;
                var.SetValue(val);
            }
            else if (type == game::ScriptVar::Type::Float)
            {
                const auto val = variant.toFloat(&success);
                if (!success)
                    return false;
                var.SetValue(val);
            }
            else if (type == game::ScriptVar::Type::Boolean)
            {
                const auto val = variant.toBool();
                var.SetValue(val);
            }
            else if (type == game::ScriptVar::Type::String)
            {
                const auto val = variant.toString();
                if (val.isNull())
                    return false;
                var.SetValue(app::ToUtf8(val));
            }
            else return false;
        }
        emit dataChanged(this->index(row, 0), this->index(row, 0));
        return true;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& var = mState.entity->GetScriptVar(index.row());
        if (role == Qt::DisplayRole)
        {
            switch (index.column()) {
                //case 0: return app::toString(var.GetType());
                case 0: return app::FromUtf8(var.GetName());
                case 1: return GetScriptVarData(var);
                default: BUG("Unknown script variable data index.");
            }
        }
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            switch (section) {
                //case 0: return "Type";
                case 0: return "Name";
                case 1: return "Value";
                default: BUG("Unknown script variable data index.");
            }
        }
        return {};
    }
    int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.entity->GetNumScriptVars());
    }
    int columnCount(const QModelIndex&) const override
    {
        //return 3;
        return 2;
    }
    void AddVariable(game::ScriptVar&& var)
    {
        const auto count = static_cast<int>(mState.entity->GetNumScriptVars());
        beginInsertRows(QModelIndex(), count, count);
        mState.entity->AddScriptVar(std::move(var));
        endInsertRows();
    }
    void EditVariable(size_t row, game::ScriptVar&& var)
    {
        mState.entity->SetScriptVar(row, std::move(var));
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void DeleteVariable(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mState.entity->DeleteScriptVar(row);
        endRemoveRows();
    }
    void Reset()
    {
        beginResetModel();
        endResetModel();
    }

private:
    QVariant GetScriptVarData(const game::ScriptVar& var) const
    {
        using app::toString;

        switch (var.GetType())
        {
            case game::ScriptVar::Type::Boolean:
                if (!var.IsArray())
                    return var.GetValue<bool>();
                return QString("[0]=%1 ...").arg(var.GetArray<bool>()[0]);
            case game::ScriptVar::Type::String:
                if (!var.IsArray())
                    return app::FromUtf8(var.GetValue<std::string>());
                return QString("[0]='%1' ...").arg(app::FromUtf8(var.GetArray<std::string>()[0]));
            case game::ScriptVar::Type::Float:
                if (!var.IsArray())
                    return QString::number(var.GetValue<float>(), 'f', 2);
                return QString("[0]=%1 ...").arg(QString::number(var.GetArray<float>()[0], 'f', 2));
            case game::ScriptVar::Type::Integer:
                if (!var.IsArray())
                    return var.GetValue<int>();
                return QString("[0]=%1 ...").arg(var.GetArray<int>()[0]);
            case game::ScriptVar::Type::Color:
            {
                if (!var.IsArray())
                {
                    const auto& color = var.GetValue<game::Color4f>();
                    return app::toString(base::ToHex(color));
                }
                const auto& color = var.GetArray<game::Color4f>()[0];
                return app::toString("[0]=%1 ...", base::ToHex(color));
            }
            break;

            case game::ScriptVar::Type::Vec2: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec2>();
                    return QString("[%1,%2]")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec2>()[0];
                    return QString("[0]=[%1,%2] ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2));
                }
            }
            case game::ScriptVar::Type::Vec3: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec3>();
                    return QString("[%1,%2,%3]")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec3>()[0];
                    return QString("[0]=[%1,%2,%3] ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2));
                }
            }
            case game::ScriptVar::Type::Vec4: {
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<glm::vec4>();
                    return QString("[%1,%2,%3,%4]")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2))
                            .arg(QString::number(val.w, 'f', 2));
                } else {
                    const auto& val = var.GetArray<glm::vec4>()[0];
                    return QString("[0]=[%1,%2,%3,%4] ...")
                            .arg(QString::number(val.x, 'f', 2))
                            .arg(QString::number(val.y, 'f', 2))
                            .arg(QString::number(val.z, 'f', 2))
                            .arg(QString::number(val.w, 'f', 2));
                }
            }

            case game::ScriptVar::Type::EntityNodeReference:
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<game::ScriptVar::EntityNodeReference>();
                    if (const auto* node = mState.entity->FindNodeById(val.id))
                        return app::FromUtf8(node->GetName());
                    return "Nil";
                } else {
                    const auto& val = var.GetArray<game::ScriptVar::EntityNodeReference>()[0];
                    if (const auto* node = mState.entity->FindNodeById(val.id))
                        return QString("[0]=%1 ...").arg(app::FromUtf8(node->GetName()));
                    return "[0]=Nil ...";
                }
                break;
            case game::ScriptVar::Type::EntityReference:
                if (!var.IsArray()) {
                    return "Nil";
                } else {
                    return "[0]=Nil ...";
                }
                break;

            case game::ScriptVar::Type::MaterialReference:
                if (!var.IsArray()) {
                    const auto& val = var.GetValue<game::ScriptVar::MaterialReference>();
                    if (const auto& material = mState.workspace->FindMaterialClassById(val.id))
                        return toString(material->GetName());
                    return "Nil";
                } else {
                    const auto& val = var.GetArray<game::ScriptVar::MaterialReference>()[0];
                    if (const auto& material = mState.workspace->FindMaterialClassById(val.id))
                        return toString("[0]=%1 ...", material->GetName());
                    return "[0]=Nil ...";
                }
                break;
        }
        BUG("Unknown ScriptVar type.");
        return {};
    }
private:
    EntityWidget::State& mState;
};

class EntityWidget::Transform3DTool : public MouseTool
{
public:
    explicit Transform3DTool(TransformGizmo3D gizmo, TransformHandle3D handle,
        gui::EntityWidget::State& state, game::EntityNodeClass* node, bool shift)
      : mTransformGizmo(gizmo)
        , mTransformHandle(handle)
        , mState(state)
        , mNode(node)
        , mShiftKey(shift)
    {}
    void Render(gfx::Painter&, gfx::Painter&) const override
    {
        const auto time_now = base::GetTime();
        const auto time_elapsed = time_now - mTime;

        if (mTransformGizmo == TransformGizmo3D::Translate)
        {
            const auto velocity = mShiftKey ? -200.0f : 200.0f; // units per second
            auto translation = GetTranslation();
            if (mTransformHandle == TransformHandle3D::XAxis)
                translation.x += velocity * time_elapsed;
            else if (mTransformHandle == TransformHandle3D::YAxis)
                translation.y += velocity * time_elapsed;
            else if (mTransformHandle == TransformHandle3D::ZAxis)
                translation.z += velocity * time_elapsed;

            SetTranslation(translation);
        }
        else if (mTransformGizmo == TransformGizmo3D::Rotate)
        {
            const auto velocity = mShiftKey ? -90.0f : 90.0f; // degrees per second

            auto rotator = GetRotation();
            auto [x, y, z] = rotator.GetEulerAngles();
            auto deg_x = x.ToDegrees();
            auto deg_y = y.ToDegrees();
            auto deg_z = z.ToDegrees();
            if (mTransformHandle == TransformHandle3D::XAxis)
                deg_x += velocity * time_elapsed;
            else if (mTransformHandle == TransformHandle3D::YAxis)
                deg_y += velocity * time_elapsed;
            else if (mTransformHandle == TransformHandle3D::ZAxis)
                deg_z += velocity * time_elapsed;

            deg_x = math::clamp(-180.0f, 180.0f, deg_x);
            deg_y = math::clamp(-180.0f, 180.0f, deg_y);
            deg_z = math::clamp(-180.0f, 180.0f, deg_z);
            SetRotation(base::Rotator::FromEulerXYZ(base::FDegrees(deg_x), base::FDegrees(deg_y), base::FDegrees(deg_z)));
        }

        mTime = time_now;
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform& view) override
    {
        mTime = base::GetTime();
        if (mTransformHandle == TransformHandle3D::Reset)
        {
            if (mTransformGizmo == TransformGizmo3D::Translate)
                SetTranslation(glm::vec3{0.0f, 0.0f, 0.0f});
            else if (mTransformGizmo == TransformGizmo3D::Rotate)
                SetRotation(game::Rotator());
        }
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        return true;
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform& view) override
    {}
    bool KeyPress(QKeyEvent* key) override
    {
        if (key->key() == Qt::Key_Shift)
        {
            mShiftKey = !mShiftKey;
            return true;
        }
        return false;
    }
private:
    game::Rotator GetRotation() const
    {
        if (const auto* drawable = mNode->GetDrawable())
            return drawable->GetRenderRotation();
        else if (const auto* text = mNode->GetTextItem())
            return text->GetRenderRotation();
        else if (const auto* light = mNode->GetBasicLight())
            return base::Rotator::FromDirection(light->GetDirection());
        BUG("No attachment for gizmo tool to work on.");
        return {};
    }
    glm::vec3 GetTranslation() const
    {
        if (const auto* drawable = mNode->GetDrawable())
            return drawable->GetRenderTranslation();
        else if (const auto* text = mNode->GetTextItem())
            return text->GetRenderTranslation();
        else if (const auto* light = mNode->GetBasicLight())
            return light->GetTranslation();
        BUG("No attachment for gizmo tool to work on.");
        return {};
    }
    void SetTranslation(const glm::vec3& value) const
    {
        if (auto* drawable = mNode->GetDrawable())
            drawable->SetRenderTranslation(value);
        else if (auto* text = mNode->GetTextItem())
            text->SetRenderTranslation(value);
        else if (auto* light = mNode->GetBasicLight())
            light->SetTranslation(value);
        else BUG("No attachment for gizmo tool to work on.");
    }
    void SetRotation(const game::Rotator& value) const
    {
        if (auto* drawable = mNode->GetDrawable())
            drawable->SetRenderRotation(value);
        else if (auto* text = mNode->GetTextItem())
            text->SetRenderRotation(value);
        else if (auto* light = mNode->GetBasicLight())
            light->SetDirection(value.ToDirectionVector());
        else BUG("No attachment for gizmo tool to work on.");
    }

private:
    const TransformGizmo3D mTransformGizmo = TransformGizmo3D::None;
    const TransformHandle3D mTransformHandle = TransformHandle3D::None;
    EntityWidget::State& mState;
    game::EntityNodeClass* mNode = nullptr;
    mutable double mTime = 0.0;
    bool mShiftKey = true;
};

class EntityWidget::SplineTool : public MouseTool
{
public:
    explicit SplineTool(gui::EntityWidget::State& state, game::EntityNodeClass* node,
                        game::SplineMoverClass* spline, size_t point_index)
      : mState(state)
      , mNode(node)
      , mSpline(spline)
      , mIndex(point_index)
    {}
    void Render(gfx::Painter& window_painter, gfx::Painter& entity_painter) const override
    {
        if (mPlacePoint)
        {
            const auto* parent = mState.entity->FindNodeParent(mNode);
            const auto& mode = mSpline->GetPathCoordinateSpace();

            const game::EntityNodeClass* coordinate_reference_node = nullptr;
            if (mode == game::SplineMoverClass::PathCoordinateSpace::Absolute)
                coordinate_reference_node = parent;
            else if (mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
                coordinate_reference_node = mNode;
            else BUG("Bug on spline path mode.");

            const auto& spline_local_point = mSpline->GetPathRelativePoint(mIndex).GetPosition();
            const auto& spline_world_point = mState.entity->MapCoordsFromNode(spline_local_point, coordinate_reference_node);
            const auto& mouse_world_point  = mState.entity->MapCoordsFromNode(mMousePosLocal, coordinate_reference_node);
            DrawLine(entity_painter, spline_world_point, mouse_world_point);
        }
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto parent_node = mState.entity->FindNodeParent(mNode);
        const auto spline_path_mode = mSpline->GetPathCoordinateSpace();
        if (spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Relative && mIndex == 0)
            return;

        const game::EntityNodeClass* coordinate_reference_node = nullptr;
        if (spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Absolute)
            coordinate_reference_node = parent_node;
        else if (spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
            coordinate_reference_node = mNode;
        else BUG("Bug on spline path mode.");

        const auto mouse_pos_world = mickey.MapToPlane();
        mMousePosLocal = mState.entity->MapCoordsToNode(mouse_pos_world, coordinate_reference_node).ToVec2();
        glm::vec2 offset = {0.0f, 0.0f};

        if (spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
        {
            offset = mSpline->GetPoint(0).GetPosition().ToVec2();
        }

        if (mickey.TestModKey(Qt::KeyboardModifier::ShiftModifier))
        {
            const auto last_index = mSpline->GetPointCount() - 1;
            if ((mIndex == 0 && spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Absolute) ||
                    (mIndex == last_index))
                mPlacePoint = true;
        }

        if (!mPlacePoint)
        {
            auto p = mSpline->GetPoint(mIndex);
            p.SetPosition(mMousePosLocal + offset);
            mState.spline_model->UpdatePoint(p, mIndex);
        }
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
    {
        if (mPlacePoint)
        {
            const auto parent_node = mState.entity->FindNodeParent(mNode);
            const auto path_mode  = mSpline->GetPathCoordinateSpace();
            const auto last_index = mSpline->GetPointCount() - 1;

            const game::EntityNodeClass* coordinate_reference_node = nullptr;
            if (path_mode == game::SplineMoverClass::PathCoordinateSpace::Absolute)
                coordinate_reference_node = parent_node;
            else if (path_mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
                coordinate_reference_node = mNode;
            else BUG("Bug on spline path mode.");

            glm::vec2 offset;
            if (path_mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
            {
                offset = mSpline->GetPoint(0).GetPosition().ToVec2();
            }


            if (mIndex == 0)
            {
                game::SplinePoint point;
                point.SetPosition(mMousePosLocal + offset);
                mState.spline_model->PrependPoint(point);
            }
            else if (mIndex == last_index)
            {
                game::SplinePoint point;
                point.SetPosition(mMousePosLocal + offset);
                mState.spline_model->AppendPoint(point);
            }
        }
        return true;
    }
private:
    EntityWidget::State& mState;
    game::EntityNodeClass* mNode = nullptr;
    game::SplineMoverClass* mSpline = nullptr;
    std::size_t mIndex = 0;
    glm::vec2 mMousePosLocal = {0.0f, 0.0f};
    bool mPlacePoint = false;
};

class EntityWidget::JointTool : public MouseTool
{
public:
    explicit JointTool(gui::EntityWidget::State& state, glm::vec2 mouse_pos)
       : mState(state)
       , mCurrent(mouse_pos)
    {}
    void Render(gfx::Painter& painter, gfx::Painter& entity) const override
    {
        if (mCurrentNode)
        {
            if (mCurrentNode == mNodeA)
                ShowMessage("Already selected!", entity);
            else if (!mCurrentNode->HasRigidBody())
                ShowMessage("No rigid body...", entity);
        }
        else if (!mNodeA)
            ShowMessage("Select node A.", entity);
        else if (!mNodeB)
            ShowMessage("Select node B.", entity);

        if (mNodeA)
        {
            const auto hit_point_node = mNodeA->GetSize() * 0.5f + mHitPointA;
            const auto hit_point_world = mState.entity->MapCoordsFromNodeBox(hit_point_node, mNodeA);
            DrawDot(entity, hit_point_world);
        }
        if (mNodeB)
        {
            const auto hit_point_node = mNodeB->GetSize() * 0.5f + mHitPointB;
            const auto hit_point_world = mState.entity->MapCoordsFromNodeBox(hit_point_node, mNodeB);
            DrawDot(entity, hit_point_world);
        }
    }

    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mCurrent = mickey.MapToPlane();

        std::vector<game::EntityNodeClass*> hit_nodes;
        std::vector<glm::vec2> hit_boxes;
        mState.entity->CoarseHitTest(mCurrent, &hit_nodes, &hit_boxes);

        if (hit_nodes.empty())
        {
            mCurrentNode = nullptr;
            return;
        }
        mCurrentNode = hit_nodes[0];
        for (auto* node : hit_nodes)
        {
            if (node->HasRigidBody())
            {
                mCurrentNode = node;
                break;
            }
        }
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        std::vector<game::EntityNodeClass*> hit_nodes;
        std::vector<glm::vec2> hit_boxes;
        mState.entity->CoarseHitTest(mCurrent, &hit_nodes, &hit_boxes);

        if (hit_nodes.empty())
            return;

        auto* node = hit_nodes[0];
        glm::vec2 hit_box = hit_boxes[0];

        for (size_t i=0; i<hit_nodes.size(); ++i)
        {
            if (hit_nodes[i]->HasRigidBody())
            {
                node = hit_nodes[i];
                hit_box = hit_boxes[i];
                break;
            }
        }
        if (!node->HasRigidBody())
            return;

        if (node == mNodeA || node == mNodeB)
            return;

        auto hit_pos = hit_box - node->GetSize() * 0.5f;
        if (!mNodeA)
        {
            mNodeA = node;
            mHitPointA = hit_pos;
        }
        else if (!mNodeB)
        {
            mNodeB = node;
            mHitPointB = hit_pos;
        }
        DEBUG("Joint tool node selection. [node='%1', pos='%2']", node->GetName(), hit_pos);
    }

    bool MouseRelease(const MouseEvent& mickey, gfx::Transform&) override
    {
         if (mNodeA && mNodeB)
             return true;

         return false;
    }

    game::EntityNodeClass* GetNodeA() const
    {
        return mNodeA;
    }
    game::EntityNodeClass* GetNodeB() const
    {
        return mNodeB;
    }

    glm::vec2 GetHitPointA() const
    {
        return mHitPointA;
    }
    glm::vec2 GetHitPointB() const
    {
        return mHitPointB;
    }
private:
    void ShowMessage(const std::string& str, gfx::Painter& p) const
    {
        gfx::FRect rect(0.0f, 0.0f, 300.0f, 20.0f);
        rect.Translate(mCurrent.x, mCurrent.y);
        rect.Translate(20.0f, 20.0f);
        gui::ShowMessage(str, rect, p);
    }

private:
    gui::EntityWidget::State& mState;

    glm::vec2 mCurrent = {0.0f, 0.0f};

    glm::vec2 mHitPointA = {0.0f, 0.0f};
    glm::vec2 mHitPointB = {0.0f, 0.0f};
    game::EntityNodeClass* mNodeA = nullptr;
    game::EntityNodeClass* mNodeB = nullptr;
    game::EntityNodeClass* mCurrentNode = nullptr;

};

class EntityWidget::PlaceLightTool : public MouseTool
{
public:
    PlaceLightTool(EntityWidget::State& state, game::BasicLightClass::LightType type)
      : mType(type)
      , mState(state)
    {
        auto klass = std::make_shared<gfx::MaterialClass>(gfx::CreateMaterialClassFromImage(res::LightIcon));
        klass->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mMaterial = gfx::CreateMaterialInstance(klass);
    }
    void Render(gfx::Painter& painter, gfx::Painter& entity) const override
    {
        gfx::Transform model;
        model.Scale(60.0f, 60.0f); // same size as in gui/drawing DrawLightIndicator
        model.Translate(mMousePos);
        model.Translate(-30.0f, -30.0f);
        entity.Draw(gfx::Rectangle(), model, *mMaterial);
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mMousePos = mickey.MapToPlane();
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto name = GenerateEntityNodeName(*mState.entity, base::FormatString("%1 Light ", mType));

        game::BasicLightClass light;
        light.SetLightType(mType);
        light.SetAmbientColor(gfx::Color4f(0.2f, 0.2f, 0.2f, 1.0f));
        light.SetTranslation(glm::vec3(0.0f, 0.0f, -100.0f));
        light.SetQuadraticAttenuation(0.00005f);

        game::EntityNodeClass node;
        node.SetBasicLight(light);
        node.SetName(name);
        node.SetTranslation(mMousePos);
        node.SetSize(100.0f, 100.0f); // irrelevant, but larger node size makes it easier to move it
        node.SetScale(1.0f, 1.0f); // irrelevant

        auto* child = mState.entity->AddNode(std::move(node));
        mState.entity->LinkChild(nullptr, child);
        mState.view->tree->Rebuild();
        mState.view->tree->SelectItemById(child->GetId());
        mState.view->basicLight->Collapse(false);
        DEBUG("Added new light '%1'", name);
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        return false;
    }
private:
    const game::BasicLightClass::LightType mType;
    EntityWidget::State& mState;
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec2 mMousePos = {0.0f, 0.0f};
    std::unique_ptr<gfx::Material> mMaterial;
};

class EntityWidget::PlaceRigidBodyTool : public MouseTool
{
public:
    explicit PlaceRigidBodyTool(EntityWidget::State& state) : mState(state)
    {
    }

    void Render(gfx::Painter& painter, gfx::Painter& entity) const override
    {
        if (!mEngaged)
        {
            gfx::FRect rect(0.0f, 0.0f, 200.0f, 20.0f);
            rect.Translate(mCurrent.x, mCurrent.y);
            rect.Translate(20.0f, 20.0f);
            ShowMessage("Click + hold to draw!", rect, entity);
            return;
        }

        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return;

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        gfx::Transform model;
        model.Scale(width, height);
        model.Translate(xpos, ypos);

        // draw a selection rect around it.
        entity.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                    gfx::CreateMaterialFromColor(gfx::Color::Green));
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mCurrent = mickey.MapToPlane();
        mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mStart = mickey.MapToPlane();
            mCurrent = mStart;
            mEngaged = true;
        }
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        ASSERT(mEngaged);

        mEngaged = false;
        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return true;

        const auto name = GenerateEntityNodeName(*mState.entity, "Static Body ");

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        game::RigidBodyClass body;
        body.SetSimulation(game::RigidBodyClass::Simulation::Static);
        body.SetCollisionShape(game::RigidBodyClass::CollisionShape::Box);

        game::DrawableItemClass draw;
        draw.SetMaterialId("_checkerboard");
        draw.SetDrawableId("_rect");

        game::EntityNodeClass node;
        node.SetDrawable(draw);
        node.SetRigidBody(body);
        node.SetName(name);
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto* child = mState.entity->AddNode(std::move(node));
        mState.entity->LinkChild(nullptr, child);
        mState.view->tree->Rebuild();
        mState.view->tree->SelectItemById(child->GetId());
        mState.view->drawable->Collapse(false);
        DEBUG("Added new  text '%1'", name);
        return true;
    }
private:
    EntityWidget::State& mState;
private:
    // the starting object position in model coordinates of the placement
    // based on the mouse position at the time.
    glm::vec2 mStart = {0.0f, 0.0f};
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec2 mCurrent = {0.0f, 0.0f};
    bool mEngaged = false;
    bool mAlwaysSquare = false;
};


class EntityWidget::PlaceTextTool : public MouseTool
{
public:
    explicit PlaceTextTool(EntityWidget::State& state) : mState(state)
    {
        gfx::TextBuffer::Text text_and_style;
        text_and_style.text       = "text";
        text_and_style.font       = "app://fonts/KomikaTitle.ttf";
        text_and_style.fontsize   = 20;
        text_and_style.lineheight = 1.0f;
        text_and_style.underline  = false;

        gfx::TextBuffer buffer(200, 200);
        buffer.SetAlignment(gfx::TextBuffer::VerticalAlignment::AlignCenter);
        buffer.SetAlignment(gfx::TextBuffer::HorizontalAlignment::AlignCenter);
        buffer.SetText(std::move(text_and_style));

        mMaterial = gfx::CreateMaterialInstance(std::move(buffer));
    }
    void Render(gfx::Painter& painter, gfx::Painter& entity) const override
    {
        if (!mEngaged)
        {
            gfx::FRect rect(0.0f, 0.0f, 200.0f, 20.0f);
            rect.Translate(mCurrent.x, mCurrent.y);
            rect.Translate(20.0f, 20.0f);
            ShowMessage("Click + hold to draw!", rect, entity);
            return;
        }

        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return;

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        gfx::Transform model;
        model.Scale(width, height);
        model.Translate(xpos, ypos);
        entity.Draw(gfx::Rectangle(), model, *mMaterial);

        // draw a selection rect around it.
        entity.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), model,
                    gfx::CreateMaterialFromColor(gfx::Color::Green));
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mCurrent = mickey.MapToPlane();
        mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mStart = mickey.MapToPlane();
            mCurrent = mStart;
            mEngaged = true;
        }
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        ASSERT(mEngaged);

        mEngaged = false;
        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return true;

        const auto name = GenerateEntityNodeName(*mState.entity, "Text ");

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        game::TextItemClass text;
        text.SetFontSize(20);
        text.SetFontName("app://fonts/KomikaTitle.ttf");
        text.SetText("Hello");
        text.SetLayer(0);

        game::EntityNodeClass node;
        node.SetTextItem(text);
        node.SetName(name);
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto* child = mState.entity->AddNode(std::move(node));
        mState.entity->LinkChild(nullptr, child);
        mState.view->tree->Rebuild();
        mState.view->tree->SelectItemById(child->GetId());
        mState.view->drawable->Collapse(false);
        DEBUG("Added new  text '%1'", name);
        return true;
    }
private:
    EntityWidget::State& mState;
private:
    // the starting object position in model coordinates of the placement
    // based on the mouse position at the time.
    glm::vec2 mStart = {0.0f, 0.0f};
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec2 mCurrent = {0.0f, 0.0f};
    bool mEngaged = false;
    bool mAlwaysSquare = false;
private:
    std::unique_ptr<gfx::Material> mMaterial;
};


class EntityWidget::PlaceShapeTool : public MouseTool
{
public:
    PlaceShapeTool(EntityWidget::State& state, QString material, QString drawable, glm::vec2 mouse_pos)
        : mState(state)
        , mMaterialId(std::move(material))
        , mDrawableId(std::move(drawable))
    {
        mDrawableClass = mState.workspace->GetDrawableClassById(mDrawableId);
        mMaterialClass = mState.workspace->GetMaterialClassById(mMaterialId);
        mMaterial = gfx::CreateMaterialInstance(mMaterialClass);
        mDrawable = gfx::CreateDrawableInstance(mDrawableClass);
        mCurrent  = mouse_pos;
    }
    PlaceShapeTool(EntityWidget::State& state,
                   std::shared_ptr<const gfx::ParticleEngineClass> preset_particle_engine,
                   std::shared_ptr<const gfx::MaterialClass> preset_particle_engine_material,
                   glm::vec2 mouse_pos)
        : mState(state)
        , mPresetParticleEngine(std::move(preset_particle_engine))
        , mPresetParticleEngineMaterial(std::move(preset_particle_engine_material))
    {
        mMaterial = gfx::CreateMaterialInstance(mPresetParticleEngineMaterial);
        mDrawable = gfx::CreateDrawableInstance(mPresetParticleEngine);
        mCurrent = mouse_pos;
    }

    void Render(gfx::Painter&, gfx::Painter& entity) const override
    {
        if (!mEngaged)
        {
            gfx::FRect rect(0.0f, 0.0f, 200.0f, 20.0f);
            rect.Translate(mCurrent.x, mCurrent.y);
            rect.Translate(20.0f, 20.0f);
            ShowMessage("Click + hold to draw!", rect, entity);
            return;
        }

        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return;

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width  = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;
        const auto spatiality = mDrawable->GetSpatialMode();

        gfx::Transform model;

        if (spatiality == gfx::SpatialMode::True3D)
            model.RotateAroundX(gfx::FDegrees(180.0f));

        model.Scale(width, height, 100.0f);
        model.Translate(xpos, ypos);

        if (spatiality == gfx::SpatialMode::True3D)
            model.Translate(width*0.5f, height*0.5f);

        if (auto* polygon = dynamic_cast<gfx::PolygonMeshInstance*>(mDrawable.get()))
        {
            const auto mesh_type = polygon->GetMeshType();
            if (mesh_type == gfx::PolygonMeshClass::MeshType::Dimetric2DRenderMesh ||
                mesh_type == gfx::PolygonMeshClass::MeshType::Isometric2DRenderMesh)
            {
                gfx::PolygonMeshInstance::Perceptual3DGeometry geometry;
                geometry.enable_perceptual_3D = true;
                polygon->SetPerceptualGeometry(geometry);
            }
        }

        entity.Draw(*mDrawable, model, *mMaterial);

        gfx::Transform rect_model;
        rect_model.Scale(width, height);
        rect_model.Translate(xpos, ypos);
        // draw a selection rect around it.
        entity.Draw(gfx::Rectangle(gfx::SimpleShapeStyle::Outline), rect_model,
                    gfx::CreateMaterialFromColor(gfx::Color::Green));
    }
    void MouseMove(const MouseEvent& mickey, gfx::Transform&) override
    {
        mCurrent = mickey.MapToPlane();
        mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
    }
    void MousePress(const MouseEvent& mickey, gfx::Transform&) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            mStart = mickey.MapToPlane();
            mCurrent = mStart;
            mEngaged = true;
        }
    }
    bool MouseRelease(const MouseEvent& mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        CommitPresetParticleEngine();

        ASSERT(mEngaged);

        mEngaged = false;
        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return true;

        const auto name = GenerateEntityNodeName(*mState.entity, "Node ");

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;
        const auto spatiality = mDrawable->GetSpatialMode();

        game::DrawableItemClass item;
        item.SetMaterialId(mMaterialClass->GetId());
        item.SetDrawableId(mDrawableClass->GetId());
        item.SetFlag(game::DrawableItemClass::Flags::DepthTest, true);
        item.SetFlag(game::DrawableItemClass::Flags::EnableLight, true);
        item.SetFlag(game::DrawableItemClass::Flags::EnableFog, true);

        if (spatiality == gfx::SpatialMode::True3D || 
            spatiality == gfx::SpatialMode::Perceptual3D)
            item.SetDepth(100.0f);

        game::EntityNodeClass node;
        node.SetDrawable(item);
        node.SetName(name);
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto* child = mState.entity->AddNode(std::move(node));
        mState.entity->LinkChild(nullptr, child);
        mState.view->tree->Rebuild();
        mState.view->tree->SelectItemById(child->GetId());
        mState.view->drawable->Collapse(false);
        DEBUG("Added new shape '%1'", name);
        return true;
    }

private:
    void CommitPresetParticleEngine()
    {
        if (!mPresetParticleEngine)
            return;

        // this checks whether the resources already exist or not.
        // if they don't then then they are are created that.
        // this means however that if the particle engine is modified
        // the subsequence preset particle placements also use the
        // modified particle engine class.
        //
        // Not using it and creating a clone of the preset would make
        // it super difficult to check later on subsequence preset
        // placement whether a non modified preset of the particle engine
        // exists but under a different resource ID. This check could be
        // done with a hash check but that would be a bit involved as well
        // since the ID contributes to the hash, so the hash computation
        // would have to ignore the ID.
        //

        if (!mState.workspace->IsValidDrawable(mPresetParticleEngine->GetId()))
        {
            app::ParticleSystemResource resource(mPresetParticleEngine->Copy(), mPresetParticleEngine->GetName());
            resource.SetProperty("material", mPresetParticleEngineMaterial->GetId());
            mState.workspace->SaveResource(resource);
        }
        mDrawableClass = mPresetParticleEngine;

        if (!mState.workspace->IsValidMaterial(mPresetParticleEngineMaterial->GetId()))
        {
            app::MaterialResource resource(mPresetParticleEngineMaterial->Copy(),
                                           mPresetParticleEngineMaterial->GetName());
            resource.SetProperty("particle-engine-class-id", mPresetParticleEngine->GetId());
            mState.workspace->SaveResource(resource);
        }
        mMaterialClass = mPresetParticleEngineMaterial;
    }
private:
    EntityWidget::State& mState;
    // the starting object position in model coordinates of the placement
    // based on the mouse position at the time.
    glm::vec2 mStart = {0.0f, 0.0f};
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec2 mCurrent = {0.0f, 0.0f};
    bool mEngaged = false;
    bool mAlwaysSquare = false;
private:
    QString mMaterialId;
    QString mDrawableId;
    std::shared_ptr<const gfx::DrawableClass> mDrawableClass;
    std::shared_ptr<const gfx::MaterialClass> mMaterialClass;
    std::unique_ptr<gfx::Material> mMaterial;
    std::unique_ptr<gfx::Drawable> mDrawable;
private:
    std::shared_ptr<const gfx::ParticleEngineClass> mPresetParticleEngine;
    std::shared_ptr<const gfx::MaterialClass> mPresetParticleEngineMaterial;
};


EntityWidget::EntityWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create EntityWidget");

    mState.entity = std::make_shared<game::EntityClass>();
    mState.entity->SetName("My Entity");
    mOriginalHash = ComputeHash();

    mRenderTree = std::make_unique<TreeModel>(*mState.entity);
    mJointModel = std::make_unique<JointModel>(mState);
    mSplineModel = std::make_unique<SplineModel>();
    mScriptVarModel = std::make_unique<ScriptVarModel>(mState);

    mUI.setupUi(this);
    mUI.scriptVarList->setModel(mScriptVarModel.get());
    mUI.jointList->setModel(mJointModel.get());
    mUI.splinePointView->setModel(mSplineModel.get());
    QHeaderView* verticalHeader = mUI.scriptVarList->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(16);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&EntityWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&EntityWidget::ZoomOut, this)); };
    mUI.widget->onMouseMove    = std::bind(&EntityWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&EntityWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&EntityWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&EntityWidget::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&EntityWidget::MouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onMouseWheel   = std::bind(&EntityWidget::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&EntityWidget::PaintScene, this, std::placeholders::_1,
                                           std::placeholders::_2);

    // create the menu for creating instances of user defined drawables
    // since there doesn't seem to be a way to do this in the designer.
    mParticleSystems = new QMenu(this);
    mParticleSystems->menuAction()->setIcon(QIcon("icons:particle.png"));
    mParticleSystems->menuAction()->setText("Particles");
    mParticleSystems->menuAction()->setToolTip(tr("Place new particle system"));
    mCustomShapes = new QMenu(this);
    mCustomShapes->menuAction()->setIcon(QIcon("icons:polygon.png"));
    mCustomShapes->menuAction()->setText("Custom Shapes");
    mCustomShapes->menuAction()->setToolTip(tr("Place new custom shape"));

    mBasicShapes2D = new QMenu(this);
    mBasicShapes2D->menuAction()->setIcon(QIcon("icons32:rectangle.png"));
    mBasicShapes2D->menuAction()->setText("Basic 2D Shapes");
    mBasicShapes2D->menuAction()->setToolTip(tr("Place new basic 2D shape"));
    mBasicShapes2D->addAction(mUI.actionNewRect);
    mBasicShapes2D->addAction(mUI.actionNewRoundRect);
    mBasicShapes2D->addAction(mUI.actionNewCircle);
    mBasicShapes2D->addAction(mUI.actionNewSemiCircle);
    mBasicShapes2D->addAction(mUI.actionNewIsoscelesTriangle);
    mBasicShapes2D->addAction(mUI.actionNewRightTriangle);
    mBasicShapes2D->addAction(mUI.actionNewTrapezoid);
    mBasicShapes2D->addAction(mUI.actionNewParallelogram);
    mBasicShapes2D->addAction(mUI.actionNewCapsule);

    mBasicShapes3D = new QMenu(this);
    mBasicShapes3D->menuAction()->setIcon(QIcon("icons32:cube.png"));
    mBasicShapes3D->menuAction()->setText("Basic 3D Shapes");
    mBasicShapes3D->menuAction()->setToolTip(tr("Place new basic 3D shape"));
    mBasicShapes3D->addAction(mUI.actionNewCone);
    mBasicShapes3D->addAction(mUI.actionNewCube);
    mBasicShapes3D->addAction(mUI.actionNewCylinder);
    mBasicShapes3D->addAction(mUI.actionNewPyramid);
    mBasicShapes3D->addAction(mUI.actionNewSphere);

    mBasicLights = new QMenu(this);
    mBasicLights->menuAction()->setIcon(QIcon("icons:light.png"));
    mBasicLights->menuAction()->setText(tr("Basic Lights"));
    mBasicLights->menuAction()->setToolTip(tr("Place new basic light"));
    mBasicLights->addAction(mUI.actionNewAmbientLight);
    mBasicLights->addAction(mUI.actionNewDirectionalLight);
    mBasicLights->addAction(mUI.actionNewPointLight);
    mBasicLights->addAction(mUI.actionNewSpotlight);

    mTextItems = new QMenu(this);
    mTextItems->menuAction()->setIcon(QIcon("icons:text.png"));
    mTextItems->menuAction()->setText(tr("Text"));
    mTextItems->menuAction()->setToolTip(tr("Place text"));
    mTextItems->addAction(mUI.actionNewText);

    mPhysItems = new QMenu(this);
    mPhysItems->menuAction()->setIcon(QIcon("icons:physics.png"));
    mPhysItems->menuAction()->setText(tr("Physics"));
    mPhysItems->menuAction()->setToolTip(tr("Place physics objects"));
    mPhysItems->addAction(mUI.actionNewStaticRigidBody);

    mButtonBar = new QToolBar(this);
    mButtonBar->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonTextBesideIcon);
    mButtonBar->setIconSize(QSize(16, 16));
    mButtonBar->addAction(mBasicShapes2D->menuAction());
    mButtonBar->addAction(mBasicShapes3D->menuAction());
    mButtonBar->addAction(mCustomShapes->menuAction());
    mButtonBar->addAction(mParticleSystems->menuAction());
    mButtonBar->addAction(mBasicLights->menuAction());
    mButtonBar->addAction(mTextItems->menuAction());
    mButtonBar->addAction(mPhysItems->menuAction());
    mUI.toolbarLayout->addWidget(mButtonBar);

    mState.workspace = workspace;
    mState.renderer.SetClassLibrary(workspace);
    mState.renderer.SetEditingMode(true);
    mState.view = &mUI;
    mState.spline_model = mSplineModel.get();

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &EntityWidget::TreeCurrentNodeChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &EntityWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &EntityWidget::TreeClickEvent);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<engine::Renderer::RenderingStyle>(mUI.cmbStyle);
    PopulateFromEnum<game::SceneProjection>(mUI.cmbSceneProjection);
    PopulateFromEnum<game::DrawableItemClass::RenderPass>(mUI.dsRenderPass);
    PopulateFromEnum<game::DrawableItemClass::CoordinateSpace>(mUI.dsCoordinateSpace);
    PopulateFromEnum<game::RigidBodyClass::Simulation>(mUI.rbSimulation);
    PopulateFromEnum<game::RigidBodyClass::CollisionShape>(mUI.rbShape);
    PopulateFromEnum<game::TextItemClass::VerticalTextAlign>(mUI.tiVAlign);
    PopulateFromEnum<game::TextItemClass::HorizontalTextAlign>(mUI.tiHAlign);
    PopulateFromEnum<game::TextItemClass::CoordinateSpace>(mUI.tiCoordinateSpace);
    PopulateFromEnum<game::SpatialNodeClass::Shape>(mUI.spnShape);
    PopulateFromEnum<game::FixtureClass::CollisionShape>(mUI.fxShape);
    PopulateFromEnum<game::LinearMoverClass::Integrator>(mUI.tfIntegrator);
    PopulateFromEnum<game::BasicLightClass::LightType>(mUI.ltType);
    PopulateFromEnum<game::TileOcclusion>(mUI.nodeTileOcclusion);
    PopulateFromEnum<game::SplineMoverClass::PathCoordinateSpace>(mUI.splineCoordSpace);
    PopulateFromEnum<game::SplineMoverClass::PathCurveType>(mUI.splineCurveType);
    PopulateFromEnum<game::SplineMoverClass::RotationMode>(mUI.splineRotation);
    PopulateFromEnum<game::SplineMoverClass::IterationMode>(mUI.splineLooping);
    PopulateFromEnum<game::MeshEffectClass::EffectType>(mUI.meshEffectType);
    PopulateFontNames(mUI.tiFontName);
    PopulateFontSizes(mUI.tiFontSize);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.cmbStyle, engine::Renderer::RenderingStyle::FlatColor);
    SetValue(mUI.zoom, 1.0f);
    SetVisible(mUI.transform, false);

    RebuildMenus();
    RebuildCombos();

    RegisterEntityWidget(this);
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();
    setWindowTitle("My Entity");

    mUI.tiFontName->lineEdit()->setReadOnly(true);

    connect(mUI.btnAddNodeItem, &QPushButton::clicked, this, [this]() {
        QPoint point;
        point.setX(0);
        point.setY(mUI.btnAddNodeItem->height());

        if (mAttachments == nullptr)
        {
            mAttachments = new QMenu(this);
            mAttachments->addAction(mUI.actionAddDrawable);
            mAttachments->addAction(mUI.actionAddTextItem);
            mAttachments->addAction(mUI.actionAddRigidBody);
            mAttachments->addAction(mUI.actionAddFixture);
            mAttachments->addAction(mUI.actionAddLight);
            mAttachments->addAction(mUI.actionAddTilemapNode);
            mAttachments->addAction(mUI.actionAddSpatialNode);
            mAttachments->addAction(mUI.actionAddLinearMover);
            mAttachments->addAction(mUI.actionAddSplineMover);
            mAttachments->addAction(mUI.actionAddMeshEffect);
        }
        mAttachments->popup(mUI.btnAddNodeItem->mapToGlobal(point));
    });


    connect(mUI.btnHamburger, &QPushButton::clicked, this, [this]() {
        if (mHamburger == nullptr)
        {
            mHamburger = new QMenu(this);
            mHamburger->addAction(mUI.chkSnap);
            mHamburger->addAction(mUI.chkShowViewport);
            mHamburger->addAction(mUI.chkShowOrigin);
            mHamburger->addAction(mUI.chkShowGrid);
            mHamburger->addAction(mUI.chkShowComments);
        }
        QPoint point;
        point.setX(0);
        point.setY(mUI.btnHamburger->width());
        mHamburger->popup(mUI.btnHamburger->mapToGlobal(point));
    });

    QTimer::singleShot(10, this, [this]() {
        mUI.widget->activateWindow();
        mUI.widget->setFocus();
    });

}

EntityWidget::EntityWidget(app::Workspace* workspace, const app::Resource& resource)
  : EntityWidget(workspace)
{
    DEBUG("Editing entity '%1'", resource.GetName());
    const game::EntityClass* content = nullptr;
    resource.GetContent(&content);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "style", mUI.cmbStyle);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    GetUserProperty(resource, "show_comments", mUI.chkShowComments);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "variables_group", mUI.variables);
    GetUserProperty(resource, "animations_group", mUI.animations);
    GetUserProperty(resource, "joints_group", mUI.joints);
    GetUserProperty(resource, "scripting_group", mUI.scripting);
    GetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    GetUserProperty(resource, "right_splitter", mUI.rightSplitter);
    GetUserProperty(resource, "node_property_group", mUI.nodePropertiesGroup);
    GetUserProperty(resource, "node_transform_group", mUI.nodeTransformGroup);
    GetUserProperty(resource, "base_properties_group", mUI.baseProperties);
    GetUserProperty(resource, "scene_projection", mUI.cmbSceneProjection);
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x) &&
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);

    mState.entity = std::make_shared<game::EntityClass>(*content);

    // load per track resource properties.
    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& track = mState.entity->GetAnimation(i);
        const auto& Id = track.GetId();
        QVariantMap properties;
        GetProperty(resource, "track_" + Id, &properties);
        mTrackProperties[Id] = properties;
    }
    // load per node comments
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        const auto& id = node.GetId();
        QString comment;
        GetProperty(resource, "comment_" + id, &comment);
        mComments[id] = comment;
    }

    // load animator properties
    if (mState.entity->HasStateController())
    {
        const auto* controller = mState.entity->GetStateController();
        const auto& Id = controller->GetId();
        QVariantMap properties;
        GetProperty(resource, "animator_" + Id, &properties);
        mAnimatorProperties[Id] = properties;
    }

    mOriginalHash = ComputeHash();

    UpdateDeletedResourceReferences();
    RebuildCombosInternal();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();

    mScriptVarModel->Reset();
    mJointModel->Reset();

    mRenderTree = std::make_unique<TreeModel>(*mState.entity);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
}

EntityWidget::~EntityWidget()
{
    DEBUG("Destroy EntityWidget");

    DeleteEntityWidget(this);
}

QString EntityWidget::GetId() const
{
    return GetValue(mUI.entityID);
}

QImage EntityWidget::TakeScreenshot() const
{
    return mUI.widget->TakeSreenshot();
}

void EntityWidget::InitializeSettings(const UISettings& settings)
{
    SetValue(mUI.chkSnap,         settings.snap_to_grid);
    SetValue(mUI.chkShowViewport, settings.show_viewport);
    SetValue(mUI.chkShowOrigin,   settings.show_origin);
    SetValue(mUI.chkShowGrid,     settings.show_grid);
    SetValue(mUI.cmbGrid,         settings.grid);
    SetValue(mUI.zoom,            settings.zoom);

    // try to make the default splitter partitions sane.
    // looks like this garbage needs to be done *after* the
    // widget has been shown (of course) so using a timer
    // hack for a hack
    QTimer::singleShot(0, this, [this]() {
        QList<int> sizes;
        sizes << mUI.leftLayout->sizeHint().width();
        sizes << mUI.center->sizeHint().width();
        sizes << mUI.rightSplitter->sizeHint().width() + 150;
        mUI.mainSplitter->setSizes(sizes);
    });
}

void EntityWidget::SetViewerMode()
{
    SetVisible(mUI.baseProperties,  false);
    SetVisible(mUI.scripting,       false);
    SetVisible(mUI.animator,        false);
    SetVisible(mUI.entity,          false);
    SetVisible(mUI.scrollArea,      false);
    SetVisible(mUI.transform,       false);
    SetVisible(mUI.lblHelp,         false);
    SetVisible(mUI.renderTree,      false);
    SetVisible(mUI.nodeProperties,  false);
    SetVisible(mUI.nodeTransform,   false);
    SetVisible(mUI.cmbGrid,         false);
    SetVisible(mUI.btnHamburger,    false);
    SetVisible(mUI.help,            false);
    SetVisible(mUI.renderTree,      false);
    SetVisible(mUI.nodeProperties,  false);
    SetVisible(mUI.nodeTransform,   false);
    SetVisible(mUI.nodeScrollArea,  false);
    SetVisible(mUI.cmbStyle,        false);
    SetVisible(mButtonBar,          false);
    SetVisible(mUI.nodeScrollAreaWidgetContents, false),

    SetValue(mUI.chkShowGrid,       false);
    SetValue(mUI.chkShowOrigin,     false);
    SetValue(mUI.chkShowViewport,   false);
    SetValue(mUI.chkShowOrigin,     false);
    SetValue(mUI.chkSnap,           false);
    SetValue(mUI.chkShowComments,   false);
    SetValue(mUI.chkShowOrigin,     false);
    SetValue(mUI.chkShowGrid,       false);

    mUI.mainSplitter->setSizes({0, 100, 0});

    mViewerMode = true;

    QTimer::singleShot(10, this, &EntityWidget::on_btnViewReset_clicked);
    on_actionPlay_triggered();
}

void EntityWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionPreview);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionSelectObject);
    bar.addAction(mUI.actionRotateObject);
    bar.addAction(mUI.actionTranslateObject);
}

void EntityWidget::AddActions(QMenu& menu)
{
    auto* create_menu = new QMenu(&menu);
    create_menu->setTitle(tr("Create"));
    create_menu->addAction(mUI.actionJointAdd);
    create_menu->addAction(mUI.actionScriptVarAdd);
    create_menu->addAction(mUI.actionAnimationAdd);

    auto* place_menu =new QMenu(&menu);
    place_menu->setTitle(tr("Place"));
    place_menu->addAction(mBasicShapes2D->menuAction());
    place_menu->addAction(mBasicShapes3D->menuAction());
    place_menu->addAction(mCustomShapes->menuAction());
    place_menu->addAction(mParticleSystems->menuAction());
    place_menu->addAction(mBasicLights->menuAction());

    auto* tool_menu = new QMenu(this);
    tool_menu->setTitle(tr("Apply Tool"));
    tool_menu->addAction(mUI.actionNewJoint);

    auto* edit_menu = new QMenu(this);
    edit_menu->setTitle(tr("Edit Script"));
    edit_menu->addAction(mUI.actionEditEntityScript);
    edit_menu->addAction(mUI.actionEditControllerScript);

    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionPreview);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addMenu(create_menu);
    menu.addMenu(place_menu);
    menu.addMenu(tool_menu);
    menu.addSeparator();
    menu.addMenu(edit_menu);
}

bool EntityWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mState.entity->IntoJson(json);
    settings.SetValue("Entity", "content", json);
    settings.SetValue("Entity", "hash", mOriginalHash);
    settings.SetValue("Entity", "camera_offset_x", mState.camera_offset_x);
    settings.SetValue("Entity", "camera_offset_y", mState.camera_offset_y);

    for (const auto& [id, props] : mTrackProperties)
    {
        settings.SetValue("Entity", "track_" + id, props);
    }

    for (const auto& [id, comment] : mComments)
    {
        settings.SetValue("Entity", "comment_" + id, comment);
    }

    for (const auto& [id, props] : mAnimatorProperties)
    {
        settings.SetValue("Entity", "animator_" + id, props);
    }

    settings.SaveWidget("Entity", mUI.scaleX);
    settings.SaveWidget("Entity", mUI.scaleY);
    settings.SaveWidget("Entity", mUI.rotation);
    settings.SaveWidget("Entity", mUI.chkShowOrigin);
    settings.SaveWidget("Entity", mUI.chkShowGrid);
    settings.SaveWidget("Entity", mUI.chkShowViewport);
    settings.SaveWidget("Entity", mUI.chkShowComments);
    settings.SaveWidget("Entity", mUI.chkSnap);
    settings.SaveWidget("Entity", mUI.cmbGrid);
    settings.SaveWidget("Entity", mUI.cmbStyle);
    settings.SaveWidget("Entity", mUI.zoom);
    settings.SaveWidget("Entity", mUI.widget);
    settings.SaveWidget("Entity", mUI.variables);
    settings.SaveWidget("Entity", mUI.animations);
    settings.SaveWidget("Entity", mUI.scripting);
    settings.SaveWidget("Entity", mUI.joints);
    settings.SaveWidget("Entity", mUI.mainSplitter);
    settings.SaveWidget("Entity", mUI.rightSplitter);
    settings.SaveWidget("Entity", mUI.nodePropertiesGroup);
    settings.SaveWidget("Entity", mUI.nodeTransformGroup);
    settings.SaveWidget("Entity", mUI.baseProperties);
    settings.SaveWidget("Entity", mUI.cmbSceneProjection);
    return true;
}
bool EntityWidget::LoadState(const Settings& settings)
{
    data::JsonObject json;
    settings.GetValue("Entity", "content", &json);
    settings.GetValue("Entity", "hash", &mOriginalHash);
    settings.GetValue("Entity", "camera_offset_x", &mState.camera_offset_x);
    settings.GetValue("Entity", "camera_offset_y", &mState.camera_offset_y);

    settings.LoadWidget("Entity", mUI.scaleX);
    settings.LoadWidget("Entity", mUI.scaleY);
    settings.LoadWidget("Entity", mUI.rotation);
    settings.LoadWidget("Entity", mUI.chkShowOrigin);
    settings.LoadWidget("Entity", mUI.chkShowGrid);
    settings.LoadWidget("Entity", mUI.chkShowViewport);
    settings.LoadWidget("Entity", mUI.chkShowComments);
    settings.LoadWidget("Entity", mUI.chkSnap);
    settings.LoadWidget("Entity", mUI.cmbGrid);
    settings.LoadWidget("Entity", mUI.cmbStyle);
    settings.LoadWidget("Entity", mUI.zoom);
    settings.LoadWidget("Entity", mUI.widget);
    settings.LoadWidget("Entity", mUI.variables);
    settings.LoadWidget("Entity", mUI.animations);
    settings.LoadWidget("Entity", mUI.scripting);
    settings.LoadWidget("Entity", mUI.joints);
    settings.LoadWidget("Entity", mUI.mainSplitter);
    settings.LoadWidget("Entity", mUI.rightSplitter);
    settings.LoadWidget("Entity", mUI.nodePropertiesGroup);
    settings.LoadWidget("Entity", mUI.nodeTransformGroup);
    settings.LoadWidget("Entity", mUI.baseProperties);
    settings.LoadWidget("Entity", mUI.cmbSceneProjection);

    game::EntityClass klass;
    if (!klass.FromJson(json))
        WARN("Failed to restore entity state.");

    auto hash  = klass.GetHash();
    mState.entity = FindSharedEntity(hash);
    if (!mState.entity)
    {
        mState.entity = std::make_shared<game::EntityClass>(std::move(klass));
        ShareEntity(mState.entity);
    }

    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& track = mState.entity->GetAnimation(i);
        const auto& id = track.GetId();

        QVariantMap properties;
        settings.GetValue("Entity", "track_" + id, &properties);
        mTrackProperties[id] = properties;
    }

    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        const auto& id = node.GetId();
        QString comment;
        settings.GetValue("Entity", "comment_" + id, &comment);
        mComments[id] = comment;
    }

    // load animator properties
    if (mState.entity->HasStateController())
    {
        const auto* state_controller = mState.entity->GetStateController();
        const auto& Id = state_controller->GetId();
        QVariantMap properties;
        settings.GetValue("Entity", "animator_" + Id, &properties);
        mAnimatorProperties[Id] = properties;
    }


    UpdateDeletedResourceReferences();
    RebuildCombosInternal();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    DisplayCurrentCameraLocation();

    mScriptVarModel->Reset();
    mJointModel->Reset();
    mRenderTree = std::make_unique<TreeModel>(*mState.entity);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    return true;
}

bool EntityWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanPaste:
            if (clipboard->GetType() == "application/json/entity/node")
                return true;
            return false;
        case Actions::CanCopy:
        case Actions::CanCut:
            if (GetCurrentNode())
                return true;
            return false;
        case Actions::CanUndo:
            return mUndoStack.size() > 1;
        case Actions::CanZoomIn:
            return CanZoomIn(mUI.zoom);
        case Actions::CanZoomOut:
            return CanZoomOut(mUI.zoom);
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return true;
        case Actions::CanScreenshot:
            return true;
    }
    return false;
}

void EntityWidget::Cut(Clipboard& clipboard)
{
    if (auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.entity->GetRenderTree();
        game::RenderTreeIntoJson(tree, [this, &clipboard](data::Writer& data, const auto* node) {
            node->IntoJson(data);
            if (const auto* comment = base::SafeFind(mComments, node->GetId()))
                clipboard.SetProperty("comment_" + node->GetId(), *comment);
            mComments.erase(node->GetId());
        }, json, node);

        clipboard.Clear();
        clipboard.SetType("application/json/entity/node");
        clipboard.SetText(json.ToString());
        NOTE("Copied JSON to application clipboard.");

        mState.entity->DeleteNode(node);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
        RealizeEntityChange(mState.entity);
    }
}
void EntityWidget::Copy(Clipboard& clipboard) const
{
    if (const auto* node = GetCurrentNode())
    {
        data::JsonObject json;
        const auto& tree = mState.entity->GetRenderTree();
        game::RenderTreeIntoJson(tree, [this, &clipboard](data::Writer& data, const auto* node) {
            node->IntoJson(data);
            if (const auto* comment = base::SafeFind(mComments, node->GetId()))
                clipboard.SetProperty("comment_" + node->GetId(), *comment);
        }, json, node);

        clipboard.Clear();
        clipboard.SetType("application/json/entity/node");
        clipboard.SetText(json.ToString());
        NOTE("Copied JSON to application clipboard.");
    }
}
void EntityWidget::Paste(const Clipboard& clipboard)
{
    if (clipboard.IsEmpty())
    {
        NOTE("Clipboard is empty.");
        return;
    }
    else if (clipboard.GetType() != "application/json/entity/node")
    {
        NOTE("No entity node JSON data found in clipboard.");
        return;
    }

    mUI.widget->setFocus();

    data::JsonObject json;
    auto [success, _] = json.ParseString(clipboard.GetText());
    if (!success)
    {
        NOTE("Clipboard JSON parse failed.");
        return;
    }

    // use a temporary vector in case there's a problem
    std::vector<std::unique_ptr<game::EntityNodeClass>> nodes;
    std::unordered_map<std::string, QString> comments;

    bool error = false;
    game::EntityClass::RenderTree tree;
    game::RenderTreeFromJson(tree, [&nodes, &error, &comments, &clipboard](const data::Reader& data) {

        game::EntityNodeClass ret;
        if (ret.FromJson(data))
        {
            auto node = std::make_unique<game::EntityNodeClass>(ret.Clone());
            node->SetName(base::FormatString("Copy of %1", ret.GetName()));

            QString comment;
            if (clipboard.GetProperty("comment_" + ret.GetId(), &comment))
                comments[node->GetId()] = comment;

            nodes.push_back(std::move(node));
            return nodes.back().get();
        }
        error = true;
        return (game::EntityNodeClass*)nullptr;
    }, json);
    if (error || nodes.empty())
    {
        NOTE("No render tree JSON found.");
        return;
    }

    // if the mouse pointer is not within the widget then adjust
    // the paste location to the center of the widget.
    QPoint mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    if (mickey.x() < 0 || mickey.x() > mUI.widget->width() ||
        mickey.y() < 0 || mickey.y() > mUI.widget->height())
        mickey = QPoint(mUI.widget->width() * 0.5, mUI.widget->height() * 0.5);

    const auto projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);
    const auto& mouse_pos_scene = MapWindowCoordinateToWorld(mUI, mState, mickey, projection);

    auto* paste_root = nodes[0].get();
    paste_root->SetTranslation(mouse_pos_scene);
    tree.LinkChild(nullptr, paste_root);

    // if we got this far, nodes should contain the nodes to be added
    // into the scene and tree should contain their hierarchy.
    for (auto& node : nodes)
    {
        // moving the unique ptr means that node address stays the same
        // thus the tree is still valid!
        mState.entity->AddNode(std::move(node));
    }
    nodes.clear();
    // walk the tree and link the nodes into the scene.
    tree.PreOrderTraverseForEach([this, &tree](game::EntityNodeClass* node) {
        if (node == nullptr)
            return;
        auto* parent = tree.GetParent(node);
        mState.entity->LinkChild(parent, node);
    });

    mComments.merge(comments);

    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(app::FromUtf8(paste_root->GetId()));
    RealizeEntityChange(mState.entity);
}

void EntityWidget::Save()
{
    on_actionSave_triggered();
}

void EntityWidget::Undo()
{
    if (mUndoStack.size() <= 1)
    {
        NOTE("No undo available.");
        return;
    }

    // if the timer has run the top of the undo stack
    // is the same copy as the actual scene object.
    if (mUndoStack.back().GetHash() == mState.entity->GetHash())
        mUndoStack.pop_back();

    // todo: how to deal with entity being changed when the
    // animation track widget is open?

    *mState.entity = mUndoStack.back();
    mUI.tree->Rebuild();
    mUndoStack.pop_back();
    mScriptVarModel->Reset();
    mJointModel->Reset();
    DisplayCurrentNodeProperties();
    NOTE("Undo!");
}

void EntityWidget::ZoomIn()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value + 0.1f);
}
void EntityWidget::ZoomOut()
{
    const float value = GetValue(mUI.zoom);
    SetValue(mUI.zoom, value - 0.1f);
}
void EntityWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void EntityWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void EntityWidget::Shutdown()
{
    if (mPreview)
    {
        mPreview->Shutdown();
        mPreview->close();
        mPreview.reset();
    }

    mUI.widget->dispose();
}
void EntityWidget::Update(double secs)
{
    mState.renderer.SetProjection(GetValue(mUI.cmbSceneProjection));
    mState.renderer.UpdateRendererState(*mState.entity);

    if (mPlayState == PlayState::Playing)
    {
        mState.renderer.Update(*mState.entity, mEntityTime, secs);
        mEntityTime += secs;
    }
    else
    {
        mState.renderer.Update(*mState.entity, 0.0f, 0.0f);
    }

    mCurrentTime += secs;

    mAnimator.Update(mUI, mState);
}
void EntityWidget::Render()
{
    // call for the widget to paint, it will set its own OpenGL context on this thread
    // and everything should be fine.
    mUI.widget->triggerPaint();
}

void EntityWidget::RunGameLoopOnce()
{
    // WARNING: Calling into PlayWindow will change the OpenGL context on *this* thread
    if (!mPreview)
        return;

    if (mPreview->IsClosed())
    {
        mPreview->SaveState("preview_window");
        mPreview->Shutdown();
        mPreview->close();
        mPreview.reset();
    }
    else
    {
        mPreview->RunGameLoopOnce();
    }
}

bool EntityWidget::HasUnsavedChanges() const
{
    if (mOriginalHash == ComputeHash())
        return false;
    return true;
}

void EntityWidget::Refresh()
{
    if (mPreview && !mPreview->IsClosed())
    {
        mPreview->NonGameTick();
    }

    // don't take an undo snapshot while the mouse tool is in
    // action.
    if (mCurrentTool)
        return;
    // don't take an undo snapshot while the node name is being
    // edited.
    if (mUI.nodeName->hasFocus())
        return;
    else if (mUI.nodeComment->hasFocus())
        return;
    else if (mUI.nodeTag->hasFocus())
        return;

    // don't take undo snapshot while continuous edits to text props
    if (mUI.tiTextColor->isDialogOpen() || mUI.tiText->hasFocus())
        return;

    if (mUndoStack.empty())
    {
        mUndoStack.push_back(*mState.entity);
    }

    const auto curr_hash = mState.entity->GetHash();
    const auto undo_hash = mUndoStack.back().GetHash();
    if (curr_hash != undo_hash)
    {
        mUndoStack.push_back(*mState.entity);
        DEBUG("Created undo copy. stack size: %1", mUndoStack.size());
    }
}

bool EntityWidget::GetStats(Stats* stats) const
{
    stats->time  = mEntityTime;
    stats->graphics.valid = true;
    stats->graphics.fps   = mUI.widget->getCurrentFPS();
    const auto& dev_stats = mUI.widget->getDeviceResourceStats();
    stats->device.static_vbo_mem_alloc    = dev_stats.static_vbo_mem_alloc;
    stats->device.static_vbo_mem_use      = dev_stats.static_vbo_mem_use;
    stats->device.dynamic_vbo_mem_alloc   = dev_stats.dynamic_vbo_mem_alloc;
    stats->device.dynamic_vbo_mem_use     = dev_stats.dynamic_vbo_mem_use;
    stats->device.streaming_vbo_mem_use   = dev_stats.streaming_vbo_mem_use;
    stats->device.streaming_vbo_mem_alloc = dev_stats.streaming_vbo_mem_alloc;
    return true;
}

bool EntityWidget::OnEscape()
{
    if (mCurrentTool)
    {
        mCurrentTool.reset();
        UncheckPlacementActions();
    }
    else if (const auto* node = GetCurrentNode())
    {
        if (mTransformGizmo != TransformGizmo3D::None)
        {
            mTransformGizmo = TransformGizmo3D::None;
            UpdateGizmos();
        }
        else if (const auto* spline = node->GetSplineMover())
        {
            const auto row = GetSelectedRow(mUI.splinePointView);
            if (row != -1)
                ClearSelection(mUI.splinePointView);
            else mUI.tree->ClearSelection();
        }
        else mUI.tree->ClearSelection();
    }
    else
    {
        on_btnViewReset_clicked();
    }
    return true;
}

bool EntityWidget::LaunchScript(const app::AnyString& id)
{
    const auto& entity_script_id = mState.entity->GetScriptFileId();
    if (entity_script_id == id)
    {
        on_actionPreview_triggered();
        return true;
    }
    if (!mState.entity->HasStateController())
        return false;

    const auto& entity_controller_script_id = mState.entity->GetStateController()->GetScriptId();
    if (entity_controller_script_id == id)
    {
        on_actionPreview_triggered();
        return true;
    }
    return false;
}

void EntityWidget::PreviewAnimation(const game::AnimationClass& animation)
{
    if (mPreview)
    {
        mPreview->Shutdown();
        mPreview->close();
        mPreview.reset();
    }

    // make a copy of the entity so that we can mess with the
    // state without affecting the class we're working on
    auto preview_entity = std::make_shared<game::EntityClass>(*mState.entity);
    preview_entity->DeleteAnimations();
    preview_entity->AddAnimation(animation);
    preview_entity->SetIdleTrackId(animation.GetId());

    engine::Engine::RendererConfig config;
    config.style = GetValue(mUI.cmbStyle);

    auto preview = std::make_unique<PlayWindow>(*mState.workspace);
    preview->LoadState("preview_window", this);
    preview->ShowWithWAR();
    preview->LoadPreview(preview_entity, GetValue(mUI.cmbSceneProjection));
    preview->ConfigurePreviewRenderer(config);
    mPreview = std::move(preview);

    NOTE("Starting animation '%1' preview", animation.GetName());
}

void EntityWidget::SaveAnimation(const game::AnimationClass& track, const QVariantMap& properties)
{
    // keep track of the associated track properties 
    // separately. these only pertain to the UI and are not
    // used by the track/animation system itself.
    mTrackProperties[track.GetId()] = properties;

    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        auto& other = mState.entity->GetAnimation(i);
        if (other.GetId() != track.GetId())
            continue;

        // copy it over.
        other = track;
        INFO("Saved animation track '%1'", track.GetName());
        NOTE("Saved animation track '%1'", track.GetName());
        DisplayEntityProperties();
        return;
    }
    // add a copy
    mState.entity->AddAnimation(track);
    INFO("Saved animation track '%1'", track.GetName());
    NOTE("Saved animation track '%1'", track.GetName());
    DisplayEntityProperties();
}

void EntityWidget::SaveStateController(const game::EntityStateControllerClass& controller, const QVariantMap& properties)
{
    mAnimatorProperties[controller.GetId()] = properties;

    mState.entity->SetStateController(controller);
    INFO("Saved entity state controller '%1'", controller.GetName());
    NOTE("Saved entity state controller '%1'", controller.GetName());
    DisplayEntityProperties();
}

void EntityWidget::on_widgetColor_colorChanged(const QColor& color)
{
    mUI.widget->SetClearColor(color);
}

void EntityWidget::on_actionPlay_triggered()
{
    // quick restart.
    if (mPlayState == PlayState::Playing)
    {
        mState.renderer.ClearPaintState();
        mEntityTime = 0.0f;
        NOTE("Restarted entity '%1' play.", mState.entity->GetName());
    }
    mPlayState = PlayState::Playing;
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}
void EntityWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    mUI.actionPause->setEnabled(false);
}
void EntityWidget::on_actionStop_triggered()
{
    mPlayState = PlayState::Stopped;
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mEntityTime = 0.0f;
    mState.renderer.ClearPaintState();
}
void EntityWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.entityName))
        return;

    app::EntityResource resource(*mState.entity, GetValue(mUI.entityName));
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "grid", mUI.cmbGrid);
    SetUserProperty(resource, "style", mUI.cmbStyle);
    SetUserProperty(resource, "snap", mUI.chkSnap);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_comments", mUI.chkShowComments);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "widget", mUI.widget);
    SetUserProperty(resource, "show_viewport", mUI.chkShowViewport);
    SetUserProperty(resource, "variables_group", mUI.variables);
    SetUserProperty(resource, "animations_group", mUI.animations);
    SetUserProperty(resource, "joints_group", mUI.joints);
    SetUserProperty(resource, "scripting_group", mUI.scripting);
    SetUserProperty(resource, "main_splitter", mUI.mainSplitter);
    SetUserProperty(resource, "right_splitter", mUI.rightSplitter);
    SetUserProperty(resource, "node_property_group", mUI.nodePropertiesGroup);
    SetUserProperty(resource, "node_transform_group", mUI.nodeTransformGroup);
    SetUserProperty(resource, "base_properties_group", mUI.baseProperties);
    SetUserProperty(resource, "scene_projection", mUI.cmbSceneProjection);

    // save the track properties.
    for (const auto& [id, props] : mTrackProperties)
    {
        SetProperty(resource, "track_" + id, props);
    }
    // save the node comments
    for (const auto& [id, comment] : mComments)
    {
        SetProperty(resource, "comment_"  + id, comment);
    }
    // save the animator properties
    for (const auto& [id, props] : mAnimatorProperties)
    {
        SetProperty(resource, "animator_" + id, props);
    }

    mState.workspace->SaveResource(resource);
    mOriginalHash = ComputeHash();
}

void EntityWidget::on_actionPreview_triggered()
{
    if (mPreview)
    {
        mPreview->ActivateWindow();
    }
    else
    {
        engine::Engine::RendererConfig config;
        config.style = GetValue(mUI.cmbStyle);

        auto preview = std::make_unique<PlayWindow>(*mState.workspace);
        preview->LoadState("preview_window", this);
        preview->ShowWithWAR();
        preview->LoadPreview(mState.entity, GetValue(mUI.cmbSceneProjection));
        preview->ConfigurePreviewRenderer(config);
        mPreview = std::move(preview);
        NOTE("Starting entity '%1' preview.", mState.entity->GetName());
    }
}

void EntityWidget::on_actionNewJoint_triggered()
{
    mCurrentTool.reset(new JointTool(mState, MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewJoint->setChecked(true);

    mUI.widget->SetCursorShape(GfxWidget::CursorShape::CrossHair);
}

void EntityWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_rect", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewRect->setChecked(true);
}
void EntityWidget::on_actionNewCircle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_circle", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewCircle->setChecked(true);
}

void EntityWidget::on_actionNewSemiCircle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_semi_circle", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewSemiCircle->setChecked(true);
}

void EntityWidget::on_actionNewIsoscelesTriangle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_isosceles_triangle", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewIsoscelesTriangle->setChecked(true);
}
void EntityWidget::on_actionNewRightTriangle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_right_triangle", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewRightTriangle->setChecked(true);
}
void EntityWidget::on_actionNewRoundRect_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_round_rect", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewRoundRect->setChecked(true);
}
void EntityWidget::on_actionNewTrapezoid_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_trapezoid", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewTrapezoid->setChecked(true);
}
void EntityWidget::on_actionNewCapsule_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_capsule", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewCapsule->setChecked(true);
}
void EntityWidget::on_actionNewParallelogram_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_parallelogram", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewParallelogram->setChecked(true);
}

void EntityWidget::on_actionNewCone_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_cone", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewCone->setCheckable(true);
}
void EntityWidget::on_actionNewCube_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_cube", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewCube->setChecked(true);
}
void EntityWidget::on_actionNewCylinder_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_cylinder", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewCylinder->setChecked(true);
}
void EntityWidget::on_actionNewPyramid_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_pyramid", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewPyramid->setChecked(true);
}
void EntityWidget::on_actionNewSphere_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "_checkerboard", "_sphere", MapMouseCursorToWorld()));

    UncheckPlacementActions();
    mUI.actionNewSphere->setChecked(true);
}

void EntityWidget::on_actionSelectObject_triggered()
{
    mTransformGizmo = TransformGizmo3D::None;
    UpdateGizmos();
}

void EntityWidget::on_actionRotateObject_triggered()
{
    if (CanApplyGizmo())
    {
        if (mTransformGizmo == TransformGizmo3D::Rotate)
        {
            mTransformGizmo = TransformGizmo3D::None;
        }
        else
        {
            mTransformGizmo = TransformGizmo3D::Rotate;
            NOTE("Activate 3D model rotate tool.");
        }
    }
    else
    {
        NOTE("The selected object doesn't have a 3D drawable.");
    }
    UpdateGizmos();
}

void EntityWidget::on_actionTranslateObject_triggered()
{
    if (CanApplyGizmo())
    {
        if (mTransformGizmo == TransformGizmo3D::Translate)
        {
            mTransformGizmo = TransformGizmo3D::None;
        }
        else
        {
            mTransformGizmo = TransformGizmo3D::Translate;
            NOTE("Activate 3D model translate tool.");
        }
    }
    else
    {
        NOTE("The selected object doesn't have a 3D drawable.");
    }
    UpdateGizmos();
}

void EntityWidget::on_actionNewAmbientLight_triggered()
{
    mCurrentTool.reset(new PlaceLightTool(mState, game::BasicLightClass::LightType::Ambient));
}
void EntityWidget::on_actionNewDirectionalLight_triggered()
{
    mCurrentTool.reset(new PlaceLightTool(mState, game::BasicLightClass::LightType::Directional));
}
void EntityWidget::on_actionNewPointLight_triggered()
{
    mCurrentTool.reset(new PlaceLightTool(mState, game::BasicLightClass::LightType::Point));
}
void EntityWidget::on_actionNewSpotlight_triggered()
{
    mCurrentTool.reset(new PlaceLightTool(mState, game::BasicLightClass::LightType::Spot));
}
void EntityWidget::on_actionNewText_triggered()
{
    mCurrentTool.reset(new PlaceTextTool(mState));
}

void EntityWidget::on_actionNewStaticRigidBody_triggered()
{
    mCurrentTool.reset(new PlaceRigidBodyTool(mState));
}

void EntityWidget::on_actionNodeDelete_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& tree = mState.entity->GetRenderTree();
        tree.ForEachChild([this](const auto* node) {
            mComments.erase(node->GetId());
        }, node);

        mState.entity->DeleteNode(node);

        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::on_actionNodeCut_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        emit RequestAction("cut");
    }
}
void EntityWidget::on_actionNodeCopy_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        emit RequestAction("copy");
    }
}

void EntityWidget::on_actionNodeVarRef_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        std::vector<ResourceListItem> entities;
        std::vector<ResourceListItem> nodes;
        for (size_t i = 0; i < mState.entity->GetNumNodes(); ++i)
        {
            const auto& node = mState.entity->GetNode(i);
            ResourceListItem item;
            item.name = node.GetName();
            item.id   = node.GetId();
            nodes.push_back(std::move(item));
        }
        QString name = app::FromUtf8(node->GetName());
        name = name.replace(' ', '_');
        name = name.toLower();
        game::ScriptVar::EntityNodeReference  ref;
        ref.id = node->GetId();

        game::ScriptVar var(app::ToUtf8(name), ref);
        var.SetPrivate(true);
        DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                         this, var);
        if (dlg.exec() == QDialog::Rejected)
            return;

        mScriptVarModel->AddVariable(std::move(var));
        SetEnabled(mUI.btnEditScriptVar, true);
        SetEnabled(mUI.btnDeleteScriptVar, true);
    }
}

void EntityWidget::on_actionNodeMoveUpLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const int layer = item->GetLayer();
            item->SetLayer(layer + 1);
        }
        if (auto* text = node->GetTextItem())
        {
            const int layer = text->GetLayer();
            text->SetLayer(layer + 1);
        }
        if (auto* light = node->GetBasicLight())
        {
            const int layer = light->GetLayer();
            light->SetLayer(layer + 1);
        }

        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::on_actionNodeMoveDownLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const int layer = item->GetLayer();
            item->SetLayer(layer - 1);

        }
        if (auto* text = node->GetTextItem())
        {
            const int layer = text->GetLayer();
            text->SetLayer(layer - 1);
        }
        if (auto* light = node->GetBasicLight())
        {
            const int layer = light->GetLayer();
            light->SetLayer(layer - 1);
        }
        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::on_actionNodeDuplicate_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        auto* dupe = mState.entity->DuplicateNode(node);
        // update the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mUI.tree->Rebuild();
        mUI.tree->SelectItemById(app::FromUtf8(dupe->GetId()));
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::on_actionNodeComment_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        QString comment;
        if (const auto* ptr = base::SafeFind(mComments, node->GetId()))
            comment = *ptr;
        bool accepted = false;
        comment = QInputDialog::getText(this,
            tr("Edit Comment"),
            tr("Comment: "), QLineEdit::Normal, comment, &accepted);
        if (!accepted)
            return;
        mComments[node->GetId()] = comment;
        SetValue(mUI.nodeComment, comment);
    }
}

void EntityWidget::on_actionNodeRename_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        QString name = app::FromUtf8(node->GetName());
        bool accepted = false;
        name = QInputDialog::getText(this,
            tr("Rename Node"),
            tr("Name: "), QLineEdit::Normal, name, &accepted);
        if (!accepted)
            return;
        node->SetName(app::ToUtf8(name));
        SetValue(mUI.nodeName, name);
        mUI.tree->Rebuild();
    }
}

void EntityWidget::on_actionNodeRenameAll_triggered()
{
    bool accepted = false;
    QString name = "Node %i";
    name = QInputDialog::getText(this,
        tr("Rename Node"),
        tr("Name: "), QLineEdit::Normal, name, &accepted);
    if (!accepted)
        return;
    for (unsigned i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        QString node_name = name;
        node_name.replace("%i", QString::number(i));
        node.SetName(app::ToUtf8(node_name));
    }
    mUI.tree->Rebuild();
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_actionScriptVarAdd_triggered()
{
    on_btnNewScriptVar_clicked();
}
void EntityWidget::on_actionScriptVarDel_triggered()
{
    on_btnDeleteScriptVar_clicked();
}
void EntityWidget::on_actionScriptVarEdit_triggered()
{
    on_btnEditScriptVar_clicked();
}

void EntityWidget::on_actionJointAdd_triggered()
{
    on_btnNewJoint_clicked();
}
void EntityWidget::on_actionJointDel_triggered()
{
    on_btnDeleteJoint_clicked();
}
void EntityWidget::on_actionJointEdit_triggered()
{
    on_btnEditJoint_clicked();
}

void EntityWidget::on_actionAnimationAdd_triggered()
{
    on_btnNewTrack_clicked();
}
void EntityWidget::on_actionAnimationDel_triggered()
{
    on_btnDeleteTrack_clicked();
}
void EntityWidget::on_actionAnimationEdit_triggered()
{
    on_btnEditTrack_clicked();
}

void EntityWidget::on_actionAddPresetParticle_triggered()
{
    DlgParticle dlg(this, mState.workspace);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mCurrentTool.reset(new PlaceShapeTool(mState,
                                          dlg.GetParticleClass(),
                                          dlg.GetMaterialClass(),
                                          MapMouseCursorToWorld()));
    mParticleSystems->menuAction()->setChecked(true);
}

void EntityWidget::on_entityName_textChanged(const QString& text)
{
    mState.entity->SetName(GetValue(mUI.entityName));
}

void EntityWidget::on_entityTag_textChanged(const QString& text)
{
    mState.entity->SetTag(GetValue(mUI.entityTag));
}

void EntityWidget::on_entityLifetime_valueChanged(double value)
{
    const bool limit_lifetime = value > 0.0;
    mState.entity->SetLifetime(GetValue(mUI.entityLifetime));
    mState.entity->SetFlag(game::EntityClass::Flags::LimitLifetime, limit_lifetime);
}

void EntityWidget::on_chkKillAtLifetime_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::KillAtLifetime, GetValue(mUI.chkKillAtLifetime));
}
void EntityWidget::on_chkKillAtBoundary_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::KillAtBoundary, GetValue(mUI.chkKillAtBoundary));
}

void EntityWidget::on_chkTickEntity_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::TickEntity, GetValue(mUI.chkTickEntity));
}
void EntityWidget::on_chkUpdateEntity_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::UpdateEntity, GetValue(mUI.chkUpdateEntity));
}
void EntityWidget::on_chkPostUpdate_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::PostUpdate, GetValue(mUI.chkPostUpdate));
}
void EntityWidget::on_chkUpdateNodes_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::UpdateNodes, GetValue(mUI.chkUpdateNodes));
}

void EntityWidget::on_chkKeyEvents_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::WantsKeyEvents, GetValue(mUI.chkKeyEvents));
}
void EntityWidget::on_chkMouseEvents_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::WantsMouseEvents, GetValue(mUI.chkMouseEvents));
}

void EntityWidget::on_btnAddIdleTrack_clicked()
{
    // todo:
}

void EntityWidget::on_btnResetIdleTrack_clicked()
{
    mState.entity->ResetIdleTrack();
    SetValue(mUI.idleTrack, -1);
}

void EntityWidget::on_btnAddScript_clicked()
{
    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    //const auto& filename = app::FromUtf8(script.GetId());
    const auto& uri  = app::toString("ws://lua/%1.lua", script.GetId());
    const auto& file = mState.workspace->MapFileToFilesystem(uri);

    if (app::FileExists(file))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File Exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(file));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    bool accepted = false;
    QString name = app::GenerateScriptVarName(GetValue(mUI.entityName), "entity");

    DlgScriptVarName dlg(this, name, "entity");
    if (dlg.exec() == QDialog::Rejected)
        return;

    name = dlg.GetName();
    if (name.isEmpty())
        return;

    QString source = GenerateEntityScriptSource(name);

    QFile::FileError err_val = QFile::FileError::NoError;
    QString err_str;
    if (!app::WriteTextFile(file, source, &err_val, &err_str))
    {
        ERROR("Failed to write file. [file='%1', err_val=%2, err_str='%3']", file, err_val, err_str);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Error Occurred");
        msg.setText(tr("Failed to write the script file. [%1]").arg(err_str));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }

    script.SetFileURI(uri);
    app::ScriptResource resource(script, GetValue(mUI.entityName));
    mState.workspace->SaveResource(resource);
    mState.entity->SetScriptFileId(script.GetId());

    auto* widget = new ScriptWidget(mState.workspace, resource);
    emit OpenNewWidget(widget);

    SetValue(mUI.scriptFile, ListItemId(script.GetId()));
    SetEnabled(mUI.btnEditScript, true);
}
void EntityWidget::on_btnEditScript_clicked()
{
    const auto& id = (QString)GetItemId(mUI.scriptFile);
    if (id.isEmpty())
        return;
    emit OpenResource(id);
}

void EntityWidget::on_btnResetScript_clicked()
{
    mState.entity->ResetScriptFile();
    SetValue(mUI.scriptFile, -1);
    SetEnabled(mUI.btnEditScript, false);
}

void EntityWidget::on_btnEditAnimator_clicked()
{
    if (!mState.entity->HasStateController())
        return;
    const auto& state_controller = mState.entity->GetStateController();

    QVariantMap props;
    if (const auto* ptr = base::SafeFind(mAnimatorProperties, state_controller->GetId()))
        props = *ptr;
    DlgAnimator dlg(this, mState.workspace, *mState.entity, *state_controller, props);
    dlg.SetEntityWidget(this);
    dlg.exec();
}

void EntityWidget::on_btnViewPlus90_clicked()
{
    mAnimator.Plus90(mUI, mState);
}

void EntityWidget::on_btnViewMinus90_clicked()
{
    mAnimator.Minus90(mUI, mState);
}

void EntityWidget::on_btnViewReset_clicked()
{
    mAnimator.Reset(mUI, mState);
    SetValue(mUI.scaleX, 1.0f);
    SetValue(mUI.scaleY, 1.0f);
}

void EntityWidget::on_btnNewTrack_clicked()
{
    // sharing the animation class object with the new animation
    // track widget.
    auto* widget = new AnimationTrackWidget(mState.workspace, mState.entity);
    widget->SetZoom(GetValue(mUI.zoom));
    widget->SetShowGrid(GetValue(mUI.chkShowGrid));
    widget->SetShowOrigin(GetValue(mUI.chkShowOrigin));
    widget->SetShowViewport(GetValue(mUI.chkShowViewport));
    widget->SetSnapGrid(GetValue(mUI.chkSnap));
    widget->SetGrid(GetValue(mUI.cmbGrid));
    widget->SetRenderingStyle(GetValue(mUI.cmbStyle));
    widget->SetProjection(GetValue(mUI.cmbSceneProjection));
    emit OpenNewWidget(widget);
}
void EntityWidget::on_btnEditTrack_clicked()
{
    auto items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    QListWidgetItem* item = items[0];
    QString id = item->data(Qt::UserRole).toString();

    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& klass = mState.entity->GetAnimation(i);
        if (klass.GetId() != app::ToUtf8(id))
            continue;
        auto it = mTrackProperties.find(klass.GetId());
        ASSERT(it != mTrackProperties.end());
        const auto& properties = (*it).second;
        auto* widget = new AnimationTrackWidget(mState.workspace, mState.entity, klass, properties);
        widget->SetZoom(GetValue(mUI.zoom));
        widget->SetShowGrid(GetValue(mUI.chkShowGrid));
        widget->SetShowOrigin(GetValue(mUI.chkShowOrigin));
        widget->SetSnapGrid(GetValue(mUI.chkSnap));
        widget->SetGrid(GetValue(mUI.cmbGrid));
        widget->SetRenderingStyle(GetValue(mUI.cmbStyle));
        emit OpenNewWidget(widget);
    }
}
void EntityWidget::on_btnDeleteTrack_clicked()
{
    const auto& items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    const auto* item = items[0];
    const auto& trackId = (std::string)GetItemId(item);

    if (mState.entity->HasIdleTrack())
    {
        if (mState.entity->GetIdleTrackId() == trackId)
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msg.setIcon(QMessageBox::Question);
            msg.setText(tr("The selected track is the current entity idle track.\n"
                           "Are you sure you want to delete it?"));
            if (msg.exec() == QMessageBox::No)
                return;
            mState.entity->ResetIdleTrack();
            SetValue(mUI.idleTrack, -1);
        }
    }
    mState.entity->DeleteAnimationById(trackId);
    // this will remove it from the widget.
    delete item;
    // delete the associated properties.
    auto it = mTrackProperties.find(trackId);
    ASSERT(it != mTrackProperties.end());
    mTrackProperties.erase(it);
}

void EntityWidget::on_btnNewScriptVar_clicked()
{
    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        nodes.push_back(std::move(item));
    }
    game::ScriptVar var("My_Var", std::string(""));
    var.SetPrivate(true);
    DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                     this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->AddVariable(std::move(var));
    SetEnabled(mUI.btnEditScriptVar, true);
    SetEnabled(mUI.btnDeleteScriptVar, true);
}
void EntityWidget::on_btnEditScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        const auto& node = mState.entity->GetNode(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        nodes.push_back(std::move(item));
    }

    // single selection for now.
    const auto index = items[0];
    game::ScriptVar var = mState.entity->GetScriptVar(index.row());
    DlgScriptVar dlg(nodes, entities, mState.workspace->ListAllMaterials(),
                     this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->EditVariable(index.row(), std::move(var));
}

void EntityWidget::on_btnDeleteScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    mScriptVarModel->DeleteVariable(index.row());
    const auto vars = mState.entity->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
}
void EntityWidget::on_btnResetLifetime_clicked()
{
    mState.entity->SetFlag(game::EntityClass::Flags::LimitLifetime, false);
    mState.entity->SetLifetime(0.0f);
    SetValue(mUI.entityLifetime, 0.0f);
}

void EntityWidget::on_btnNewJoint_clicked()
{
    const auto index = mState.entity->GetNumJoints();
    {
        game::EntityClass::PhysicsJoint joint;
        joint.id     = base::RandomString(10);
        joint.name   = "My Joint";
        joint.type   = game::EntityClass::PhysicsJointType::Distance;
        joint.params = game::EntityClass::DistanceJointParams {};
        mJointModel->AddJoint(std::move(joint));
    }
    auto& joint = mState.entity->GetJoint(index);

    DlgJoint dlg(this, *mState.entity, joint);
    if (dlg.exec() == QDialog::Rejected)
    {
        mJointModel->DeleteJoint(index);
        return;
    }

    mJointModel->UpdateJoint(index);
    SetEnabled(mUI.btnEditJoint, true);
    SetEnabled(mUI.btnDeleteJoint, true);
}
void EntityWidget::on_btnEditJoint_clicked()
{
    auto items = mUI.jointList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];

    auto backup = mState.entity->GetJoint(index.row());

    auto& joint = mState.entity->GetJoint(index.row());
    DlgJoint dlg(this, *mState.entity, joint);
    if (dlg.exec() == QDialog::Rejected)
    {
        mJointModel->EditJoint(index.row(), std::move(backup));
        return;
    }

    mJointModel->UpdateJoint(index.row());

}
void EntityWidget::on_btnDeleteJoint_clicked()
{
    auto items = mUI.jointList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    mJointModel->DeleteJoint(index.row());
    const auto joints = mState.entity->GetNumJoints();
    SetEnabled(mUI.btnEditJoint, joints > 0);
    SetEnabled(mUI.btnDeleteJoint, joints > 0);
}

void EntityWidget::on_btnSelectMaterial_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* drawable = node->GetDrawable())
        {
            DlgMaterial dlg(this, mState.workspace, true);
            dlg.SetSelectedMaterialId(drawable->GetMaterialId());
            if (drawable->HasActiveTextureMap())
                dlg.SetSelectedTextureMapId(drawable->GetActiveTextureMap());
            else if (auto material = mState.workspace->FindMaterialClassById(drawable->GetMaterialId()))
            {
                dlg.SetSelectedTextureMapId(material->GetActiveTextureMap());
            }
            if (dlg.exec() == QDialog::Rejected)
                return;
            const auto& material_id = dlg.GetSelectedMaterialId();
            const auto& texture_map_id = dlg.GetSelectedTextureMapId();
            if (drawable->GetMaterialId() == material_id && drawable->GetActiveTextureMap() == texture_map_id)
                return;
            drawable->ResetMaterial();
            drawable->SetMaterialId(material_id);
            drawable->SetActiveTextureMap(texture_map_id);
            DisplayCurrentNodeProperties();
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_btnSetMaterialParams_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* drawable = node->GetDrawable())
        {
            const auto& material = mState.workspace->GetMaterialClassById(drawable->GetMaterialId());
            DlgMaterialParams dlg(this, drawable);
            dlg.AdaptInterface(mState.workspace, material.get());
            dlg.exec();
        }
    }
}

void EntityWidget::on_btnEditDrawable_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& id = (QString)GetItemId(mUI.dsDrawable);
        if (id.isEmpty())
            return;
        auto& resource = mState.workspace->GetResourceById(id);
        if (resource.IsPrimitive())
            return;
        emit OpenResource(id);
    }
}

void EntityWidget::on_btnEditMaterial_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const auto& id = (QString)GetItemId(mUI.dsMaterial);
        if (id.isEmpty())
            return;
        auto& resource = mState.workspace->GetResourceById(id);
        if (resource.IsPrimitive())
            return;
        emit OpenResource(id);
    }
}



void EntityWidget::on_btnMoreViewportSettings_clicked()
{
    const auto visible = mUI.transform->isVisible();
    SetVisible(mUI.transform, !visible);
    if (!visible)
        mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::DownArrow);
    else mUI.btnMoreViewportSettings->setArrowType(Qt::ArrowType::UpArrow);
}

void EntityWidget::on_trackList_itemSelectionChanged()
{
    auto list = mUI.trackList->selectedItems();
    mUI.btnEditTrack->setEnabled(!list.isEmpty());
    mUI.btnDeleteTrack->setEnabled(!list.isEmpty());
}

void EntityWidget::on_idleTrack_currentIndexChanged(int index)
{
    if (index == -1)
    {
        mState.entity->ResetIdleTrack();
        return;
    }
    mState.entity->SetIdleTrackId(GetItemId(mUI.idleTrack));
}

void EntityWidget::on_scriptFile_currentIndexChanged(int index)
{
    if (index == -1)
    {
        mState.entity->ResetScriptFile();
        SetEnabled(mUI.btnEditScript, false);
        SetEnabled(mUI.btnResetScript, false);
        return;
    }
    mState.entity->SetScriptFileId(GetItemId(mUI.scriptFile));
    SetEnabled(mUI.btnEditScript, true);
}

void EntityWidget::on_nodeName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    else if (!item->GetUserData())
        return;
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    node->SetName(app::ToUtf8(text));
    item->SetText(text);
    mUI.tree->Update();
    RebuildCombosInternal();
    RealizeEntityChange(mState.entity);
}
void EntityWidget::on_nodeComment_textChanged(const QString& text)
{
    if (const auto* node = GetCurrentNode())
    {
        if (text.isEmpty())
            mComments.erase(node->GetId());
        else mComments[node->GetId()] = text;
    }
}

void EntityWidget::on_nodeTag_textChanged(const QString& text)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetTag(GetValue(mUI.nodeTag));
    }
}

void EntityWidget::on_nodeIndex_valueChanged(int)
{
    if (const auto* node = GetCurrentNode())
    {
        const size_t src_index = mState.entity->FindNodeIndex(node);
        ASSERT(src_index < mState.entity->GetNumNodes());

        const size_t dst_index = GetValue(mUI.nodeIndex);
        if (dst_index >= mState.entity->GetNumNodes())
        {
            SetValue(mUI.nodeIndex, src_index);
            return;
        }
        mState.entity->MoveNode(src_index, dst_index);
    }
}

void EntityWidget::on_nodeSizeX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeSizeY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeTranslateX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeTranslateY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeScaleX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeScaleY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodePlus90_clicked()
{
    const float value = GetValue(mUI.nodeRotation);
    SetValue(mUI.nodeRotation, math::clamp(-180.0f, 180.0f, value + 90.0f));
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_nodeMinus90_clicked()
{
    const float value = GetValue(mUI.nodeRotation);
    SetValue(mUI.nodeRotation,math::clamp(-180.0f, 180.0f, value - 90.0f));
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsDrawable_currentIndexChanged(const QString& name)
{
    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::on_dsMaterial_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        auto* drawable = node->GetDrawable();
        if (drawable)
            drawable->ClearMaterialParams();
    }

    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::on_dsRenderPass_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsCoordinateSpace_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsLayer_valueChanged(int value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsTimeScale_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsDepth_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsXRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsYRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsZRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsXOffset_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsYOffset_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsZOffset_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsVisible_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsUpdateDrawable_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsUpdateMaterial_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsRestartDrawable_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsFlipHorizontally_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsFlipVertically_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsBloom_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsLights3D_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsFog3D_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsDoubleSided_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsDepthTest_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbSimulation_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_rbPolygon_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbFriction_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbRestitution_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbAngularDamping_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbLinearDamping_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbDensity_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsBullet_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsSensor_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbCanSleep_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbDiscardRotation_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiFontName_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiFontSize_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiVAlign_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiHAlign_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiTextColor_colorChanged(QColor color)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiLineHeight_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiLayer_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiRasterWidth_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiRasterHeight_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiCoordinateSpace_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiXRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiYRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiZRotation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiXTranslation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiYTranslation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiZTranslation_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiText_textChanged()
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiVisible_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiUnderline_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiBlink_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiStatic_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiBloom_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiLights_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiFog_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tiDepthTest_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_spnShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_spnEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_fxShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_fxBody_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxPolygon_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxFriction_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxDensity_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_fxBounciness_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_fxIsSensor_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_btnResetFxFriction_clicked()
{
    SetValue(mUI.fxFriction,  mUI.fxFriction->minimum());
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_btnResetFxDensity_clicked()
{
    SetValue(mUI.fxDensity, mUI.fxDensity->minimum());
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_btnResetFxBounciness_clicked()
{
    SetValue(mUI.fxBounciness, mUI.fxBounciness->minimum());
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_btnSelectFont_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* text = node->GetTextItem())
        {
            DlgFont::DisplaySettings disp;
            disp.font_size  = text->GetFontSize();
            disp.text_color = FromGfx(text->GetTextColor());
            disp.underline  = text->TestFlag(game::TextItemClass::Flags::UnderlineText);
            disp.blinking   = text->TestFlag(game::TextItemClass::Flags::BlinkText);
            DlgFont dlg(this, mState.workspace, text->GetFontName(), disp);
            if (dlg.exec() == QDialog::Rejected)
                return;
            SetValue(mUI.tiFontName, dlg.GetSelectedFontURI());
            text->SetFontName(dlg.GetSelectedFontURI());
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_btnSelectFontFile_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* text = node->GetTextItem())
        {
            const auto& name = QFileDialog::getOpenFileName(this ,
                tr("Select Font File") , "" ,
                tr("Font (*.ttf *.otf *.json)"));
            if (name.isEmpty())
                return;
            const auto& file = mState.workspace->MapFileToWorkspace(name);
            SetValue(mUI.tiFontName , file);
            text->SetFontName(file);
            RealizeEntityChange(mState.entity);
        }
    }
}

void EntityWidget::on_btnResetTextRasterWidth_clicked()
{
    SetValue(mUI.tiRasterWidth, 0);
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_btnResetTextRasterHeight_clicked()
{
    SetValue(mUI.tiRasterHeight, 0);
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_mnVCenter_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_mnHCenter_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_nodeMapLayer_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_nodeTileOcclusion_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tfIntegrator_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_tfVelocityX_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfVelocityY_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfVelocityA_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfAccelX_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfAccelY_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfAccelA_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tfRotate_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltType_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltAmbient_colorChanged(const QColor&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_ltDiffuse_colorChanged(const QColor&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_ltSpecular_colorChanged(const QColor&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_ltConstantAttenuation_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_ltLinearAttenuation_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_ltQuadraticAttenuation_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltTranslation_ValueChanged(const Vector3*)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltDirection_ValueChanged(const Vector3*)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltSpotHalfAngle_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltLayer_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_ltEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_splineCoordSpace_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_splineCurveType_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_splineRotation_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_splineLooping_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_splineSpeed_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_splineAcceleration_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_splineFlagEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_meshEffectType_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_meshEffectShape_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
    if (auto* node = GetCurrentNode())
    {
        if (auto* effect = node->GetMeshEffect())
        {
            SetEnabled(mUI.btnResetEffectShape, effect->HasEffectShapeId());
            SetEnabled(mUI.shardIterations, !effect->HasEffectShapeId());
        }
    }
}

void EntityWidget::on_shardIterations_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_shardLinearVelo_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_shardLinearAccel_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_shardRotVelo_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_shardRotAccel_valueChanged(double)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_btnResetEffectShape_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* mesh = node->GetMeshEffect())
        {
            mesh->ResetEffectShapeId();
            DisplayCurrentNodeProperties();
        }
    }
}

void EntityWidget::on_btnDelDrawable_clicked()
{
    ToggleDrawable(false);
}

void EntityWidget::ToggleLight(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasBasicLight())
            {
                game::BasicLightClass light;
                light.SetAmbientColor(gfx::Color4f(0.2f, 0.2f, 0.2f, 1.0f));
                light.SetTranslation(glm::vec3{0.0f, 0.0f, -100.0f});
                light.SetQuadraticAttenuation(0.00005f);
                node->SetBasicLight(light);

                ScrollEntityNodeArea();

                DEBUG("Added light to '%1'.", node->GetName());
            }
        }
        else
        {
            node->RemoveBasicLight();
            DEBUG("Removed light from '%1'.", node->GetName());
        }
        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);

        mUI.basicLight->Collapse(!on);
    }
}

void EntityWidget::ToggleDrawable(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasDrawable())
            {
                game::DrawableItemClass draw;
                draw.SetMaterialId("_checkerboard");
                draw.SetDrawableId("_rect");
                node->SetDrawable(draw);

                ScrollEntityNodeArea();

                DEBUG("Added drawable item to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveDrawable();
            DEBUG("Removed drawable item from '%1'", node->GetName());
        }
        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);

        mUI.drawable->Collapse(!on);
    }
}

void EntityWidget::on_btnDelRigidBody_clicked()
{
    ToggleRigidBody(false);
}

void EntityWidget::ToggleRigidBody(bool on)
{
    auto* node = GetCurrentNode();
    if (!node)
        return;
    if (!on)
    {
        node->RemoveRigidBody();
        DEBUG("Removed rigid body from '%1'" , node->GetName());
    }
    else if (!node->HasRigidBody())
    {
        game::RigidBodyClass body;
        // try to see if we can figure out the right collision
        // box for this rigid body based on the drawable.
        if (auto* item = node->GetDrawable())
        {
            const auto& drawableId = item->GetDrawableId();
            if (drawableId == "_circle")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::Circle);
            else if (drawableId == "_parallelogram")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::Parallelogram);
            else if (drawableId == "_rect" || drawableId == "_round_rect")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::Box);
            else if (drawableId == "_isosceles_triangle")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::IsoscelesTriangle);
            else if (drawableId == "_right_triangle")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::RightTriangle);
            else if (drawableId == "_trapezoid")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::Trapezoid);
            else if (drawableId == "_semi_circle")
                body.SetCollisionShape(game::RigidBodyClass::CollisionShape::SemiCircle);
            else if (auto klass = mState.workspace->FindDrawableClassById(drawableId)) {
                if (klass->GetType() == gfx::DrawableClass::Type::Polygon) {
                    body.SetPolygonShapeId(drawableId);
                    body.SetCollisionShape(game::RigidBodyClass::CollisionShape::Polygon);
                }
            }
        }
        node->SetRigidBody(body);

        ScrollEntityNodeArea();

        DEBUG("Added rigid body to '%1'", node->GetName());
    }

    mState.entity->DeleteInvalidJoints();
    mState.entity->DeleteInvalidFixtures();
    mJointModel->Reset();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
    RebuildCombosInternal();
    RealizeEntityChange(mState.entity);

    mUI.rigidBody->Collapse(!on);
}

void EntityWidget::on_btnDelTextItem_clicked()
{
    ToggleTextItem(false);
}

void EntityWidget::ToggleTextItem(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasTextItem())
            {
                int layer = 0;
                if (const auto* draw = node->GetDrawable())
                    layer = draw->GetLayer() + 1;

                // Select some font as a default. Without this the font is an
                // empty string which will not render any text (but rather print
                // a cascade of crap in the debug/error logs)
                SetValue(mUI.tiFontName, 0);

                game::TextItemClass text;
                text.SetFontSize(GetValue(mUI.tiFontSize));
                text.SetFontName(GetValue(mUI.tiFontName));
                text.SetText("Hello");
                text.SetLayer(layer);
                node->SetTextItem(text);

                ScrollEntityNodeArea();

                DEBUG("Added text item to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveTextItem();
            DEBUG("Removed text item from '%1'" , node->GetName());
        }
        RealizeEntityChange(mState.entity);
        DisplayCurrentNodeProperties();

        mUI.textItem->Collapse(!on);
    }
}

void EntityWidget::on_btnDelSpatialNode_clicked()
{
    ToggleSpatialNode(false);
}

void EntityWidget::on_btnDelLinearMover_clicked()
{
    ToggleLinearMover(false);
}

void EntityWidget::on_btnDelSplineMover_clicked()
{
    ToggleSplineMover(false);
}

void EntityWidget::on_btnDelLight_clicked()
{
    ToggleLight(false);
}

void EntityWidget::on_btnDelMeshEffect_clicked()
{
    ToggleMeshEffect(false);
}

void EntityWidget::on_actionAddLight_triggered()
{
    ToggleLight(true);
}

void EntityWidget::on_actionAddDrawable_triggered()
{
    ToggleDrawable(true);
}
void EntityWidget::on_actionAddTextItem_triggered()
{
    ToggleTextItem(true);
}
void EntityWidget::on_actionAddRigidBody_triggered()
{
    ToggleRigidBody(true);
}
void EntityWidget::on_actionAddFixture_triggered()
{
    ToggleFixture(true);
}
void EntityWidget::on_actionAddTilemapNode_triggered()
{
    ToggleTilemapNode(true);
}
void EntityWidget::on_actionAddSpatialNode_triggered()
{
    ToggleSpatialNode(true);
}
void EntityWidget::on_actionAddLinearMover_triggered()
{
    ToggleLinearMover(true);
}

void EntityWidget::on_actionAddSplineMover_triggered()
{
    ToggleSplineMover(true);
}

void EntityWidget::on_actionAddMeshEffect_triggered()
{
    ToggleMeshEffect(true);
}

void EntityWidget::on_actionEditEntityScript_triggered()
{
    on_btnEditScript_clicked();
}
void EntityWidget::on_actionEditControllerScript_triggered()
{
    if (!mState.entity->HasStateController())
        return;

    const auto* state_controller = mState.entity->GetStateController();

    const auto& id = state_controller->GetScriptId();
    if (id.empty())
        return;

    ActionEvent::OpenResource open;
    open.id = app::FromUtf8(id);
    ActionEvent::Post(open);
}


void EntityWidget::ToggleSpatialNode(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasSpatialNode())
            {
                SetValue(mUI.spnShape, game::SpatialNodeClass::Shape::AABB);

                game::SpatialNodeClass sp;
                sp.SetShape(game::SpatialNodeClass::Shape::AABB);
                sp.SetFlag(game::SpatialNodeClass::Flags::Enabled, GetValue(mUI.spnEnabled));
                node->SetSpatialNode(sp);

                ScrollEntityNodeArea();

                DEBUG("Added spatial node to '%1'.", node->GetName());
            }
        }
        else
        {
            node->RemoveSpatialNode();
            DEBUG("Removed spatial node from '%1'.", node->GetName());
        }
        DisplayCurrentNodeProperties();

        mUI.spatialNode->Collapse(!on);
    }
}

void EntityWidget::on_btnDelFixture_clicked()
{
    ToggleFixture(false);
}

void EntityWidget::ToggleFixture(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on && !node->HasFixture())
        {
            game::FixtureClass fixture;
            // try to see if we can figure out the right collision
            // box for this rigid body based on the drawable.
            if (auto* item = node->GetDrawable())
            {
                const auto& drawableId = item->GetDrawableId();
                if (drawableId == "_circle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Circle);
                else if (drawableId == "_parallelogram")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Parallelogram);
                else if (drawableId == "_rect" || drawableId == "_round_rect")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Box);
                else if (drawableId == "_isosceles_triangle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::IsoscelesTriangle);
                else if (drawableId == "_right_triangle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::RightTriangle);
                else if (drawableId == "_trapezoid")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Trapezoid);
                else if (drawableId == "_semi_circle")
                    fixture.SetCollisionShape(game::FixtureClass::CollisionShape::SemiCircle);
                else if (auto klass = mState.workspace->FindDrawableClassById(drawableId)) {
                    if (klass->GetType() == gfx::DrawableClass::Type::Polygon)
                    {
                        fixture.SetPolygonShapeId(drawableId);
                        fixture.SetCollisionShape(game::FixtureClass::CollisionShape::Polygon);
                    }
                }
            }
            node->SetFixture(fixture);

            ScrollEntityNodeArea();

            DEBUG("Added fixture to '%1'.", node->GetName());
        }
        else if (!on && node->HasFixture())
        {
            node->RemoveFixture();
            DEBUG("Removed fixture from '%1'.", node->GetName());
        }
        DisplayCurrentNodeProperties();

        mUI.fixture->Collapse(!on);
    }
}

void EntityWidget::on_btnDelTilemapNode_clicked()
{
    ToggleTilemapNode(false);
}

void EntityWidget::ToggleTilemapNode(bool on)
{
    if(auto* node = GetCurrentNode())
    {
        if (on && !node->HasMapNode())
        {
            game::MapNodeClass map;
            node->SetMapNode(map);

            ScrollEntityNodeArea();

            DEBUG("Added map node to '%1'", node->GetName());
        }
        else if (!on && node->HasMapNode())
        {
            node->RemoveMapNode();
        }
        DisplayCurrentNodeProperties();

        mUI.tilemapNode->Collapse(!on);
    }
}

void EntityWidget::ToggleLinearMover(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on && !node->HasLinearMover())
        {
            game::LinearMoverClass mover;
            node->SetLinearMover(mover);

            ScrollEntityNodeArea();

            DEBUG("Added linear mover to node '%1'", node->GetName());
        }
        else if (!on && node->HasLinearMover())
        {
            node->RemoveLinearMover();
        }
        DisplayCurrentNodeProperties();

        mUI.linearMover->Collapse(!on);
    }
}

void EntityWidget::ToggleSplineMover(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on && !node->HasSplineMover())
        {
            game::SplineMoverClass mover;
            mover.SetPathCoordinateSpace(game::SplineMoverClass::PathCoordinateSpace::Absolute);
            mover.SetSpeed(100.0f);

            glm::vec2 point = {-200.0f, 0.0f};
            for(unsigned i=0; i<=4; ++i)
            {
                game::SplinePoint p;
                p.SetPosition(point);

                mover.AppendPoint(p);
                point += glm::vec2{100.0f, 0.0f};
            }

            node->SetSplineMover(mover);

            ScrollEntityNodeArea();

            DEBUG("Added spline mover to node '%1'", node->GetName());
        }
        else if (!on && node->HasSplineMover())
        {
            node->RemoveSplineMover();
        }
        DisplayCurrentNodeProperties();

        mUI.splineMover->Collapse(!on);
    }
}

void EntityWidget::ToggleMeshEffect(bool on)
{
    if (auto* node = GetCurrentNode())
    {
         if (on && !node->HasMeshEffect())
         {
             game::MeshEffectClass::MeshExplosionEffectArgs args;
             args.mesh_subdivision_count = 1;
             args.shard_linear_speed = 1.0f;
             args.shard_linear_acceleration = 2.0f;
             args.shard_rotational_speed = 1.0f;
             args.shard_rotational_acceleration = 2.0f;

             game::MeshEffectClass effect;
             effect.SetEffectType(game::MeshEffectClass::EffectType::MeshExplosion);
             effect.SetEffectArgs(args);

             node->SetMeshEffect(effect);
         }
        else if (!on && node->HasMeshEffect())
        {
            node->RemoveMeshEffect();
        }
        DisplayCurrentNodeProperties();

        mUI.meshEffect->Collapse(!on);
    }
}

void EntityWidget::on_animator_toggled(bool on)
{
    if (on)
    {
        game::EntityStateControllerClass animator;
        animator.SetName("My EntityStateController");
        mState.entity->SetStateController(std::move(animator));
    }
    else
    {
        mState.entity->DeleteStateController();
    }
}

void EntityWidget::on_tree_customContextMenuRequested(QPoint)
{
    const auto* node = GetCurrentNode();
    const auto* item = node ? node->GetDrawable() : nullptr;
    const auto* text = node ? node->GetTextItem() : nullptr;
    const auto* light = node ? node->GetBasicLight() : nullptr;
    const auto count = mState.entity->GetNumNodes();

    SetEnabled(mUI.actionNodeMoveDownLayer, item != nullptr || text != nullptr || light != nullptr);
    SetEnabled(mUI.actionNodeMoveUpLayer, item != nullptr || text != nullptr || light != nullptr);
    SetEnabled(mUI.actionNodeDelete, node != nullptr);
    SetEnabled(mUI.actionNodeDuplicate, node != nullptr);
    SetEnabled(mUI.actionNodeVarRef, node != nullptr);
    SetEnabled(mUI.actionNodeComment, node != nullptr);
    SetEnabled(mUI.actionNodeRename, node != nullptr);
    SetEnabled(mUI.actionNodeRenameAll, count > 0);
    SetEnabled(mUI.actionNodeCopy, node != nullptr);
    SetEnabled(mUI.actionNodeCut, node != nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDuplicate);
    menu.addAction(mUI.actionNodeRename);
    menu.addAction(mUI.actionNodeRenameAll);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeVarRef);
    menu.addAction(mUI.actionNodeComment);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeCut);
    menu.addAction(mUI.actionNodeCopy);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDelete);
    menu.exec(QCursor::pos());
}

void EntityWidget::on_scriptVarList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionScriptVarAdd);
    menu.addAction(mUI.actionScriptVarEdit);
    menu.addAction(mUI.actionScriptVarDel);
    menu.exec(QCursor::pos());
}

void EntityWidget::on_jointList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionJointAdd);
    menu.addAction(mUI.actionJointEdit);
    menu.addAction(mUI.actionJointDel);
    menu.exec(QCursor::pos());
}

void EntityWidget::on_trackList_customContextMenuRequested(QPoint)
{
    QMenu menu(this);
    menu.addAction(mUI.actionAnimationAdd);
    menu.addAction(mUI.actionAnimationEdit);
    menu.addAction(mUI.actionAnimationDel);
    menu.exec(QCursor::pos());
}

void EntityWidget::ScrollEntityNodeArea()
{
    QTimer::singleShot(100, this, [this]() {
        auto* scroll = mUI.nodeScrollArea->verticalScrollBar();
        const auto max = scroll->maximum();
        scroll->setValue(max);
    });
}

void EntityWidget::TreeCurrentNodeChangedEvent()
{
    DisplayCurrentNodeProperties();
    mTransformGizmo = TransformGizmo3D::None;
    mTransformHandle = TransformHandle3D::None;
    UpdateGizmos();
}
void EntityWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.entity->GetRenderTree();
    auto* src_value = static_cast<game::EntityNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::EntityNodeClass*>(target->GetUserData());

    // check if we're trying to drag a parent onto its own child
    if (game::SearchChild(tree, dst_value, src_value))
        return;
    const bool retain_world_transform = true;
    mState.entity->ReparentChild(dst_value, src_value, retain_world_transform);
}
void EntityWidget::TreeClickEvent(TreeWidget::TreeItem* item, unsigned icon_index)
{
    //DEBUG("Tree click event: %1, icon=%2", item->GetId(), icon_index);
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    if (node == nullptr)
        return;

    if (icon_index == 0)
    {
        const bool visibility = !node->TestFlag(game::EntityNodeClass::Flags::VisibleInEditor);
        node->SetFlag(game::EntityNodeClass::Flags::VisibleInEditor, visibility);
        item->SetVisibilityIcon(visibility ? QIcon() : QIcon("icons:crossed_eye.png"));
    }
    else if (icon_index == 1)
    {
        const bool locked = !node->TestFlag(game::EntityNodeClass::Flags::LockedInEditor);
        node->SetFlag(game::EntityNodeClass::Flags::LockedInEditor, locked);
        item->SetLockedIcon(locked ? QIcon("icons:lock.png") : QIcon());
        SetEnabled(mUI.nodeTransform, !locked);
    }
    mUI.tree->Update();
}

void EntityWidget::OnAddResource(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::OnRemoveResource(const app::Resource* resource)
{
    UpdateDeletedResourceReferences();
    RebuildCombos();
    RebuildMenus();
    DisplayEntityProperties();
    DisplayCurrentNodeProperties();
}
void EntityWidget::OnUpdateResource(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
    RealizeEntityChange(mState.entity);
    mState.renderer.ClearPaintState();
}

void EntityWidget::PlaceNewParticleSystem()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the data in the action as the class id of the drawable.
    const auto drawable = action->data().toString();
    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResourceById(drawable);
    QString material = resource.GetProperty("material",  QString("_checkerboard"));
    if (!mState.workspace->IsValidMaterial(material))
        material = "_checkerboard";

    mCurrentTool = std::make_unique<PlaceShapeTool>(mState, material, drawable, MapMouseCursorToWorld());
}
void EntityWidget::PlaceNewCustomShape()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the data in the action as the name of the drawable.
    const auto drawable = action->data().toString();
    // check the resource in order to get the default material name set in the
    // shape editor.
    const auto& resource = mState.workspace->GetResourceById(drawable);
    QString material = resource.GetProperty("material",  QString("_checkerboard"));
    if (!mState.workspace->IsValidMaterial(material))
        material = "_checkerboard";

    mCurrentTool = std::make_unique<PlaceShapeTool>(mState, material, drawable, MapMouseCursorToWorld());
}

void EntityWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    // WARNING, if you use the preview window here to draw the underlying
    // OpenGL Context will change unexpectedly and then drawing below
    // will trigger OpenGL errors.

    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto scene_projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);

    SetValue(mUI.widgetColor, mUI.widget->GetCurrentClearColor());

    gfx::Device* device = painter.GetDevice();
    gfx::Painter entity_painter(device);
    entity_painter.SetViewMatrix(CreateViewMatrix(mUI, mState, scene_projection));
    entity_painter.SetProjectionMatrix(CreateProjectionMatrix(mUI,  engine::Projection::Orthographic));
    entity_painter.SetPixelRatio(glm::vec2{xs*zoom, ys*zoom});
    entity_painter.SetViewport(0, 0, width, height);
    entity_painter.SetSurfaceSize(width, height);

    DrawHook draw_hook(GetCurrentNode());
    draw_hook.SetDrawVectors(true);
    draw_hook.SetDrawSelectionBox(true);
    draw_hook.SetIsPlaying(mPlayState == PlayState::Playing);
    draw_hook.SetSceneProjection(scene_projection);
    draw_hook.SetTransformGizmo(mTransformGizmo);
    draw_hook.SetTransformHandle(mTransformHandle);

    if (mTransformGizmo != TransformGizmo3D::None)
    {
        draw_hook.SetDrawVectors(false);
    }

    const auto camera_position = glm::vec2{mState.camera_offset_x, mState.camera_offset_y};
    const auto camera_scale    = glm::vec2{xs, ys};
    const auto camera_rotation = (float)GetValue(mUI.rotation);

    LowLevelRenderHook low_level_render_hook(
            camera_position,
            camera_scale,
            scene_projection,
            camera_rotation,
            width, height,
            zoom,
            grid,
            GetValue(mUI.chkShowGrid));

    engine::Renderer::Camera camera;
    camera.clear_color = mUI.widget->GetCurrentClearColor();
    camera.position    = camera_position;
    camera.rotation    = camera_rotation;
    camera.scale       = camera_scale * zoom;
    camera.viewport    = game::FRect(-width*0.5f, -height*0.5f, width, height);
    camera.ppa         = engine::ComputePerspectiveProjection(camera.viewport);
    mState.renderer.SetCamera(camera);

    engine::Renderer::Surface surface;
    surface.viewport = gfx::IRect(0, 0, width, height);
    surface.size     = gfx::USize(width, height);
    mState.renderer.SetSurface(surface);

    mState.renderer.SetLowLevelRendererHook(&low_level_render_hook);
    mState.renderer.SetStyle(GetValue(mUI.cmbStyle));
    mState.renderer.SetClassLibrary(mState.workspace);
    mState.renderer.SetEditingMode(true);
    mState.renderer.SetName("EntityWidgetRenderer/" + mState.entity->GetId());
    mState.renderer.SetProjection(scene_projection);

    mState.renderer.BeginFrame();
    mState.renderer.CreateFrame(*mState.entity, &draw_hook);
    mState.renderer.DrawFrame(*device);
    mState.renderer.EndFrame();

    // Draw joints, drawn in the entity space.
    for (size_t i=0; i<mState.entity->GetNumJoints(); ++i)
    {
        const auto& joint = mState.entity->GetJoint(i);
        if (!joint.IsValid())
            continue;

        const auto* src_node = mState.entity->FindNodeById(joint.src_node_id);
        const auto* dst_node = mState.entity->FindNodeById(joint.dst_node_id);
        if (!src_node || !dst_node)
            continue;

        const auto& src_anchor_point_local = src_node->GetSize() * 0.5f + joint.src_node_anchor_point;
        const auto& dst_anchor_point_local = dst_node->GetSize() * 0.5f + joint.dst_node_anchor_point;
        const auto& src_anchor_point_world = mState.entity->MapCoordsFromNodeBox(src_anchor_point_local, src_node);
        const auto& dst_anchor_point_world = mState.entity->MapCoordsFromNodeBox(dst_anchor_point_local, dst_node);

        if (joint.type == game::EntityClass::PhysicsJointType::Distance)
        {
            // const auto& params = std::get<game::EntityClass::DistanceJointParams>(joint.params);

            DrawLine(entity_painter, src_anchor_point_world, dst_anchor_point_world);
            DrawDot(entity_painter, src_anchor_point_world);
            DrawDot(entity_painter, dst_anchor_point_world);
        }
        else if (joint.type == game::EntityClass::PhysicsJointType::Revolute)
        {
            //const auto& params = std::get<game::EntityClass::RevoluteJointParams>(joint.params);

            DrawDot(entity_painter, src_anchor_point_world);
        }
        else if (joint.type == game::EntityClass::PhysicsJointType::Weld)
        {
            //const auto& params = std::get<game::EntityClass::WeldJointParams>(joint.params);

            DrawDot(entity_painter, src_anchor_point_world);
        }
        else if (joint.type == game::EntityClass::PhysicsJointType::Prismatic)
        {
            const auto& params = std::get<game::EntityClass::PrismaticJointParams>(joint.params);

            const auto& direction_vector_local = game::RotateVectorAroundZ(glm::vec2(1.0f, 0.0f), params.direction_angle.ToRadians());
            const auto& direction_vector_world = game::TransformDirection(mState.entity->FindNodeTransform(src_node), direction_vector_local);

            DrawDot(entity_painter, src_anchor_point_world);
            DrawDir(entity_painter, src_anchor_point_world, game::FindVectorRotationAroundZ(direction_vector_world));
        }
        else if (joint.type == game::EntityClass::PhysicsJointType::Pulley)
        {
            const auto& params = std::get<game::EntityClass::PulleyJointParams>(joint.params);
            const auto* world_anchor_node_a = mState.entity->FindNodeById(params.anchor_nodes[0]);
            const auto* world_anchor_node_b = mState.entity->FindNodeById(params.anchor_nodes[1]);
            if (!world_anchor_node_a || !world_anchor_node_b)
                continue;

            const auto& anchor_node_a_world = mState.entity->MapCoordsFromNodeBox(world_anchor_node_a->GetSize() * 0.5f, world_anchor_node_a);
            const auto& anchor_node_b_world = mState.entity->MapCoordsFromNodeBox(world_anchor_node_b->GetSize() * 0.5f, world_anchor_node_b);
            DrawLine(entity_painter, src_anchor_point_world, anchor_node_a_world);
            DrawLine(entity_painter, dst_anchor_point_world, anchor_node_b_world);
            DrawLine(entity_painter, anchor_node_a_world, anchor_node_b_world);
            DrawDot(entity_painter, src_anchor_point_world);
            DrawDot(entity_painter, dst_anchor_point_world);
            DrawDot(entity_painter, anchor_node_a_world);
            DrawDot(entity_painter, anchor_node_b_world);
        }
    }

    // Draw comments, drawn in entity space
    if (GetValue(mUI.chkShowComments))
    {
        for (const auto&[id, comment]: mComments)
        {
            if (comment.isEmpty())
                continue;
            if (const auto* node = mState.entity->FindNodeById(id))
            {
                const auto& size = node->GetSize();
                const auto& pos = mState.entity->MapCoordsFromNodeBox({0.0f, size.y}, node);
                ShowMessage(comment, gfx::FPoint(pos.x + 10, pos.y + 10), entity_painter);
            }
        }
    }

    if (const auto* node = GetCurrentNode())
    {
        if (const auto* map = node->GetMapNode())
        {
            const auto has_focus = mUI.mnVCenter->hasFocus() || mUI.mnHCenter->hasFocus();
            if (has_focus)
            {
                const auto& center = map->GetSortPoint();
                const auto& size = node->GetSize();
                const auto& pos = mState.entity->MapCoordsFromNodeBox(size * center, node);
                gfx::Transform model;
                model.MoveTo(pos);
                model.Resize(10.0f, 10.0f);
                model.Translate(-5.0f, -5.0f);
                entity_painter.Draw(gfx::Circle(), model, gfx::CreateMaterialFromColor(gfx::Color::HotPink));
            }
        }
        if (const auto* spline = node->GetSplineMover())
        {
            const auto* parent = mState.entity->FindNodeParent(node);
            const auto& mode = spline->GetPathCoordinateSpace();

            const game::EntityNodeClass* coordinate_reference_node = nullptr;
            if (mode == game::SplineMoverClass::PathCoordinateSpace::Absolute)
                coordinate_reference_node = parent;
            else if (mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
                coordinate_reference_node = node;
            else BUG("Bug on spline path mode.");

            DrawSpline(entity_painter, spline, coordinate_reference_node, *mState.entity);

            for (int i=0; i<spline->GetPointCount(); ++i)
            {
                const auto selected_row = GetSelectedRow(mUI.splinePointView);
                const auto& spline_local_point = spline->GetPathRelativePoint(i).GetPosition();
                const auto& spline_world_point = mState.entity->MapCoordsFromNode(spline_local_point, coordinate_reference_node);

                DrawSplineControlPoint(entity_painter, spline_world_point, selected_row == i);
            }
        }
    }

    if (mCurrentTool)
    {
        mCurrentTool->Render(painter, entity_painter);
        if (const auto* ptr = dynamic_cast<const Transform3DTool*>(mCurrentTool.get()))
        {
            DisplayCurrentNodeProperties();
        }
    }

    if (mState.entity->GetNumNodes() == 0)
    {
        ShowInstruction(
            "Create a new game play entity.\n\n"
            "INSTRUCTIONS\n"
            "1. Select a shape in the main tool bar above.\n"
            "2. Click & hold left mouse button to draw.\n"
            "3. Adjust the shape properties in the panel on the right.\n\n\n"
            "Hit 'Play' to animate materials and shapes.\n"
            "Hit 'Test Run' to test the entity.\n"
            "Hit 'Save' to save the entity.",
            gfx::FRect(0, 0, float(width), float(height)),
            painter, 28
        );
        return;
    }

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        gfx::Transform view;
        DrawBasisVectors(entity_painter, view);
    }

    if (GetValue(mUI.chkShowViewport))
    {
        gfx::Transform view;
        MakeViewTransform(mUI, mState, view);
        const auto& settings = mState.workspace->GetProjectSettings();
        const float game_width = settings.viewport_width;
        const float game_height = settings.viewport_height;
        DrawViewport(painter, view, game_width, game_height, width, height);
    }

    PrintMousePos(mUI, mState, painter, scene_projection);

    if (mTransformGizmo != TransformGizmo3D::None && !mCurrentTool)
    {
        const auto& mouse_point = mUI.widget->mapFromGlobal(QCursor::pos());
        if (mouse_point.x() < 0 || mouse_point.x() >= width)
            return;
        if (mouse_point.y() < 0 || mouse_point.y() >= height)
            return;

        const auto* current = GetCurrentNode();
        const auto node_box_size = current->GetSize();
        const auto& node_to_world = mState.entity->FindNodeTransform(current);

        // limit the pixel sampling by checking against the tool hotspot
        // and only proceed to check for the transform handle axis if the
        // mouse pointer is currently inside the current node's selection box.
        gfx::FRect box;
        box.Resize(node_box_size.x, node_box_size.y);
        box.Translate(-node_box_size.x * 0.5f, -node_box_size.y * 0.5f);

        if (game::IsAxonometricProjection(scene_projection))
        {
            // fudge it a little bit in case of dimetric projection since the box
            // doesn't actually cover the whole 3D renderable object.
            box.Grow(20.0f, 20.0f);
            box.Translate(-10.0f, -10.0f);
        }
        const auto hotspot = TestToolHotspot(mUI, mState, node_to_world, box, mouse_point, scene_projection);
        if (hotspot != ToolHotspot::Remove)
        {
            mTransformHandle = TransformHandle3D::None;
            return;
        }

        // going to take a shortcut here and just read a pixel under the
        // mouse that use that to determine that transform handle.
        // This could be wrong if there's another object occluding the
        // currently selected object, and it happens to have the same color
        // as on of the transform axis we're going to check against.
        // A better solution would require some selective rendering similar
        // to what we have in the scene widget.
        const auto& bitmap = device->ReadColorBuffer(mouse_point.x(), height - mouse_point.y(), 1, 1);
        const auto& pixel = bitmap.GetPixel(0, 0);
        if (pixel == gfx::Color::Green)
            mTransformHandle = TransformHandle3D::XAxis;
        else if (pixel == gfx::Color::Red)
            mTransformHandle = TransformHandle3D::YAxis;
        else if (pixel == gfx::Color::Blue)
            mTransformHandle = TransformHandle3D::ZAxis;
        else if (pixel == gfx::Color::White)
            mTransformHandle = TransformHandle3D::Reset;
        else if (pixel != gfx::Color::Yellow)
            mTransformHandle = TransformHandle3D::None;
    }
}

void EntityWidget::MouseZoom(const std::function<void(void)>& zoom_function)
{
    if (gui::MouseZoom(mUI, mState, zoom_function))
        DisplayCurrentCameraLocation();
}

void EntityWidget::MouseMove(QMouseEvent* event)
{
    if (mCurrentTool)
    {
        const auto projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);
        const MouseEvent mickey(event, mUI, mState, projection);

        mCurrentTool->MouseMove(mickey);

        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        DisplayCurrentNodeProperties();
    }
}
void EntityWidget::MousePress(QMouseEvent* event)
{
    const auto projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);
    const MouseEvent mickey(event, mUI, mState, projection);

    if (!mCurrentTool && !mViewerMode && (mickey->button() == Qt::RightButton))
    {
        if (auto* current = GetCurrentNode())
        {
            if (mTransformGizmo != TransformGizmo3D::None && mTransformHandle != TransformHandle3D::None)
            {
                mCurrentTool.reset(new Transform3DTool(mTransformGizmo, mTransformHandle, mState, current, true));
            }
        }
    }

    if (!mCurrentTool && !mViewerMode && (mickey->button() == Qt::LeftButton))
    {
        const auto snap = (bool)GetValue(mUI.chkSnap);
        const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
        const auto grid_size = static_cast<unsigned>(grid);

        if (auto* current = GetCurrentNode())
        {
            if (auto* spline = current->GetSplineMover())
            {
                const auto* parent_node = mState.entity->FindNodeParent(current);
                const auto spline_path_mode = spline->GetPathCoordinateSpace();
                const game::EntityNodeClass* coordinate_reference_node = nullptr;

                if (spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Absolute)
                    coordinate_reference_node = parent_node;
                else if (spline_path_mode == game::SplineMoverClass::PathCoordinateSpace::Relative)
                    coordinate_reference_node = current;
                else BUG("Bug on spline path mode.");

                for (size_t i=0; i<spline->GetPointCount(); ++i)
                {
                    const auto& spline_local_point = spline->GetPathRelativePoint(i).GetPosition();
                    const auto& spline_world_point = mState.entity->MapCoordsFromNode(spline_local_point, coordinate_reference_node);
                    gfx::FRect box;
                    box.Resize(20.0f, 20.0f);
                    box.Move(spline_world_point);
                    box.Translate(-10.0f, -10.0f);
                    if (box.TestPoint(mickey.MapToPlane()))
                    {
                        mCurrentTool = std::make_unique<SplineTool>(mState, current, spline, i);
                        SelectRow(mUI.splinePointView, i);
                        break;
                    }
                }
            }
            if (mTransformGizmo != TransformGizmo3D::None && mTransformHandle != TransformHandle3D::None)
            {
                mCurrentTool.reset(new Transform3DTool(mTransformGizmo, mTransformHandle, mState, current, false));
            }

            if (!mCurrentTool)
            {
                const auto node_box_size = current->GetSize();
                const auto& node_to_world = mState.entity->FindNodeTransform(current);
                gfx::FRect box;
                box.Resize(node_box_size.x, node_box_size.y);
                box.Translate(-node_box_size.x * 0.5f, -node_box_size.y * 0.5f);

                const auto hotspot = TestToolHotspot(mUI, mState, node_to_world, box, mickey->pos(), projection);
                if (hotspot == ToolHotspot::Resize)
                    mCurrentTool.reset(new ResizeRenderTreeNodeTool(*mState.entity, current, snap, grid_size));
                else if (hotspot == ToolHotspot::Rotate)
                    mCurrentTool.reset(new RotateRenderTreeNodeTool(*mState.entity, current));
                else if (hotspot == ToolHotspot::Remove)
                    mCurrentTool.reset(new MoveRenderTreeNodeTool(*mState.entity, current, snap, grid_size));
                else mUI.tree->ClearSelection();
            }
        }

        if (!GetCurrentNode())
        {
            auto [hitnode, hitpos] = SelectNode(mickey.MapToPlane(), *mState.entity, GetCurrentNode());
            if (hitnode)
            {
                const auto node_box_size = hitnode->GetSize();
                const auto& node_to_world = mState.entity->FindNodeTransform(hitnode);
                gfx::FRect box;
                box.Resize(node_box_size.x, node_box_size.y);
                box.Translate(-node_box_size.x*0.5f, -node_box_size.y*0.5f);

                const auto hotspot = TestToolHotspot(mUI, mState, node_to_world, box, mickey->pos(), projection);
                if (hotspot == ToolHotspot::Resize)
                    mCurrentTool.reset(new ResizeRenderTreeNodeTool(*mState.entity, hitnode, snap, grid_size));
                else if (hotspot == ToolHotspot::Rotate)
                    mCurrentTool.reset(new RotateRenderTreeNodeTool(*mState.entity, hitnode));
                else if (hotspot == ToolHotspot::Remove)
                    mCurrentTool.reset(new MoveRenderTreeNodeTool(*mState.entity, hitnode, snap, grid_size));

                mUI.tree->SelectItemById(hitnode->GetId());
                mTransformGizmo = TransformGizmo3D::None;
                mTransformHandle = TransformHandle3D::None;
                UpdateGizmos();
            }
        }
    }
    else if (!mCurrentTool && (mickey->button() == Qt::RightButton))
    {
        mCurrentTool.reset(new PerspectiveCorrectCameraTool(mUI, mState));
    }

    if (mCurrentTool)
    {
        if (mCurrentTool->GetToolFunction() == MouseTool::ToolFunctionType::TransformNode &&
            GetCurrentNode()->TestFlag(game::EntityNodeClass::Flags::LockedInEditor))
        {
            NOTE("Unlock node to apply node transformations.");
            mCurrentTool.reset();
        } else mCurrentTool->MousePress(mickey);
    }

}
void EntityWidget::MouseRelease(QMouseEvent* event)
{
    if (!mCurrentTool)
        return;

    const auto projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);
    const MouseEvent mickey(event, mUI, mState, projection);

    if (mCurrentTool->MouseRelease(mickey))
    {
        if (const auto* joint_tool = dynamic_cast<JointTool*>(mCurrentTool.get()))
        {
            const auto* nodeA = joint_tool->GetNodeA();
            const auto* nodeB = joint_tool->GetNodeB();

            const auto index = mState.entity->GetNumJoints();
            {
                game::EntityClass::PhysicsJoint joint;
                joint.id          = base::RandomString(10);
                joint.name        = "My Joint";
                joint.type        = game::EntityClass::PhysicsJointType::Distance;
                joint.params      = game::EntityClass::DistanceJointParams {};
                joint.type        = game::EntityClass::PhysicsJointType::Distance;
                joint.src_node_id = nodeA->GetId();
                joint.dst_node_id = nodeB->GetId();
                joint.src_node_anchor_point = joint_tool->GetHitPointA();
                joint.dst_node_anchor_point = joint_tool->GetHitPointB();
                mJointModel->AddJoint(std::move(joint));
            }
            auto& joint = mState.entity->GetJoint(index);

            DlgJoint dlg(this, *mState.entity, joint);
            if (dlg.exec() == QDialog::Rejected)
            {
                mJointModel->DeleteJoint(index);
            }
            else
            {
                mJointModel->UpdateJoint(index);
                SetEnabled(mUI.btnEditJoint, true);
                SetEnabled(mUI.btnDeleteJoint, true);
            }
        }

        mCurrentTool.reset();
        UncheckPlacementActions();
        DisplayCurrentNodeProperties();
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::MouseDoubleClick(QMouseEvent* event)
{
    const auto projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);
    const MouseEvent mickey(event, mUI, mState, projection);

    if (const auto* ptr = dynamic_cast<const Transform3DTool*>(mCurrentTool.get()))
        return;

    // double click is preceded by a regular click event and quick
    // googling suggests that there's really no way to filter out
    // single click when trying to react only to double click other
    // than to set a timer (which adds latency).
    // Going to simply discard any tool selection here on double click.
    mCurrentTool.reset();
    UncheckPlacementActions();

    auto [hitnode, hitpos] = SelectNode(mickey.MapToPlane(), *mState.entity, GetCurrentNode());
    if (!hitnode)
        return;

    const bool did_have_focus = mUI.widget->hasFocus() || mUI.widget->hasInputFocus();
    mTransformGizmo = TransformGizmo3D::None;
    mTransformHandle = TransformHandle3D::None;
    UpdateGizmos();

    if (auto* drawable = hitnode->GetDrawable())
    {
        on_btnSelectMaterial_clicked();
    }
    else if (auto* text = hitnode->GetTextItem())
    {
        on_btnSelectFont_clicked();
    }

    // losing focus when opening the dialog, try to
    // restore the focus.
    if (did_have_focus)
    {
        QTimer::singleShot(10, this, [this]() {
            mUI.widget->activateWindow();
            mUI.widget->setFocus();
        });
    }
}

void EntityWidget::MouseWheel(QWheelEvent* wheel)
{
    // we know this is for zoom
    if (wheel->modifiers() & Qt::ControlModifier)
        return;

    if (auto* node = GetCurrentNode())
    {
        if (auto* draw = node->GetDrawable())
        {
            const auto& id = draw->GetMaterialId();
            const auto& mat = mState.workspace->FindMaterialClassById(id);
            if (mat == nullptr)
                return;
            if (mat->GetType() != gfx::MaterialClass::Type::Tilemap)
                return;

            int index = 0;
            if (const auto* ptr = draw->GetMaterialParamValue<float>("kTileIndex"))
                index = *ptr;

            const QPoint& num_degrees = wheel->angleDelta() / 8;
            const QPoint& num_steps = num_degrees / 15;
            if (num_steps.y() > 0)
                --index;
            else ++index;

            index = std::max(index, 0);
            draw->SetMaterialParam("kTileIndex", static_cast<float>(index));
        }
    }
}

bool EntityWidget::KeyPress(QKeyEvent* event)
{
    // handle key press events coming from the gfx widget
    if (mCurrentTool && mCurrentTool->KeyPress(event))
        return true;

    const auto key = event->key();
    const auto shift= event->modifiers() & Qt::Key_Shift;

    switch (key)
    {
        case Qt::Key_Delete:
            if (auto* node = GetCurrentNode())
            {
                auto* spline = node->GetSplineMover();
                const int spline_point_index = spline ? GetSelectedRow(mUI.splinePointView) : -1;
                if (spline && spline_point_index != -1)
                {
                    const auto spline_point_count = spline->GetPointCount();
                    if (spline_point_count > 4)
                        mSplineModel->ErasePoint(spline_point_index);
                    else NOTE("Spline needs a minimum of 4 control points.");
                }
                else
                {
                    on_actionNodeDelete_triggered();
                }
            }
            break;
        case Qt::Key_T:
            if (shift)
            {
                SelectTile();
            }
            else if (CanApplyGizmo())
            {
                on_actionTranslateObject_triggered();
            }
            break;
        case Qt::Key_R:
            if (CanApplyGizmo())
            {
                on_actionRotateObject_triggered();
            }
            break;

        case Qt::Key_W:
            TranslateCamera(0.0f, 20.0f);
            break;
        case Qt::Key_S:
            TranslateCamera(0.0f, -20.0f);
            break;
        case Qt::Key_A:
            TranslateCamera(20.0f, 0.0f);
            break;
        case Qt::Key_D:
            TranslateCamera(-20.0f, 0.0f);
            break;
        case Qt::Key_Left:
            TranslateCurrentNode(-20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            TranslateCurrentNode(20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            TranslateCurrentNode(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            TranslateCurrentNode(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            OnEscape();
            break;
        case Qt::Key_Space:
            if (auto* node = GetCurrentNode())
            {
                if (event->modifiers() & Qt::Key_Shift)
                {
                    if (auto* drawable = node->GetDrawable())
                        on_btnSelectMaterial_clicked();
                    else if (auto* text = node->GetTextItem())
                        on_btnSelectFont_clicked();
                }
                else
                {
                    if (auto* text = node->GetTextItem())
                        on_btnSelectFont_clicked();
                    else if (auto* drawable = node->GetDrawable())
                        on_btnSelectMaterial_clicked();
                }
                mUI.widget->setFocus();
            }
            break;
        default:
            return false;
    }

    return true;
}

void EntityWidget::DisplayEntityProperties()
{
    std::vector<ResourceListItem> tracks;
    for (size_t i=0; i< mState.entity->GetNumAnimations(); ++i)
    {
        const auto& track = mState.entity->GetAnimation(i);
        ResourceListItem item;
        item.name = track.GetName();
        item.id   = track.GetId();
        item.icon = QIcon("icons:animation_track.png");
        tracks.push_back(std::move(item));
    }
    SetList(mUI.trackList, tracks);
    SetList(mUI.idleTrack, tracks);

    const auto vars   = mState.entity->GetNumScriptVars();
    const auto joints = mState.entity->GetNumJoints();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteTrack, false);
    SetEnabled(mUI.btnEditTrack, false);
    SetEnabled(mUI.btnEditJoint, joints > 0);
    SetEnabled(mUI.btnDeleteJoint, joints > 0);
    SetEnabled(mUI.btnEditScript, mState.entity->HasScriptFile());

    SetValue(mUI.animator, mState.entity->HasStateController());
    SetValue(mUI.entityName, mState.entity->GetName());
    SetValue(mUI.entityTag, mState.entity->GetTag());
    SetValue(mUI.entityID, mState.entity->GetId());
    SetValue(mUI.idleTrack, ListItemId(mState.entity->GetIdleTrackId()));
    SetValue(mUI.scriptFile, ListItemId(mState.entity->GetScriptFileId()));
    SetValue(mUI.entityLifetime, mState.entity->TestFlag(game::EntityClass::Flags::LimitLifetime)
                                 ? mState.entity->GetLifetime() : 0.0f);
    SetValue(mUI.chkKillAtLifetime, mState.entity->TestFlag(game::EntityClass::Flags::KillAtLifetime));
    SetValue(mUI.chkKillAtBoundary, mState.entity->TestFlag(game::EntityClass::Flags::KillAtBoundary));
    SetValue(mUI.chkTickEntity, mState.entity->TestFlag(game::EntityClass::Flags::TickEntity));
    SetValue(mUI.chkUpdateEntity, mState.entity->TestFlag(game::EntityClass::Flags::UpdateEntity));
    SetValue(mUI.chkPostUpdate, mState.entity->TestFlag(game::EntityClass::Flags::PostUpdate));
    SetValue(mUI.chkUpdateNodes, mState.entity->TestFlag(game::EntityClass::Flags::UpdateNodes));
    SetValue(mUI.chkKeyEvents, mState.entity->TestFlag(game::EntityClass::Flags::WantsKeyEvents));
    SetValue(mUI.chkMouseEvents, mState.entity->TestFlag(game::EntityClass::Flags::WantsMouseEvents));

    if (!mUI.trackList->selectedItems().isEmpty())
    {
        SetEnabled(mUI.btnDeleteTrack, true);
        SetEnabled(mUI.btnEditTrack, true);
    }
}

void EntityWidget::DisplayCurrentNodeProperties()
{
    SetValue(mUI.nodeID, QString(""));
    SetValue(mUI.nodeName, QString(""));
    SetValue(mUI.nodeTag, QString(""));
    SetValue(mUI.nodeComment, QString(""));
    SetValue(mUI.nodeIndex, 0);
    SetValue(mUI.nodeTranslateX, 0.0f);
    SetValue(mUI.nodeTranslateY, 0.0f);
    SetValue(mUI.nodeSizeX, 0.0f);
    SetValue(mUI.nodeSizeY, 0.0f);
    SetValue(mUI.nodeScaleX, 1.0f);
    SetValue(mUI.nodeScaleY, 1.0f);
    SetValue(mUI.nodeRotation, 0.0f);
    SetValue(mUI.dsMaterial, -1);
    SetValue(mUI.dsDrawable, -1);
    SetValue(mUI.dsLayer, 0);
    SetValue(mUI.dsRenderPass, -1);
    SetValue(mUI.dsCoordinateSpace, -1);
    SetValue(mUI.dsTimeScale, 1.0f);
    SetValue(mUI.dsDepth, 0.0f);
    SetValue(mUI.dsXRotation, 0.0f);
    SetValue(mUI.dsYRotation, 0.0f);
    SetValue(mUI.dsZRotation, 0.0f);
    SetValue(mUI.dsXOffset, 0.0f);
    SetValue(mUI.dsYOffset, 0.0f);
    SetValue(mUI.dsZOffset, 0.0f);
    SetValue(mUI.rbShape, -1);
    SetValue(mUI.rbFriction, 0.0f);
    SetValue(mUI.rbRestitution, 0.0f);
    SetValue(mUI.rbAngularDamping, 0.0f);
    SetValue(mUI.rbLinearDamping, 0.0f);
    SetValue(mUI.rbDensity, 0.0f);
    SetValue(mUI.rbIsBullet, false);
    SetValue(mUI.rbIsSensor, false);
    SetValue(mUI.rbIsEnabled, false);
    SetValue(mUI.rbCanSleep, false);
    SetValue(mUI.rbDiscardRotation, false);
    SetValue(mUI.tiFontName, -1);
    SetValue(mUI.tiFontSize, 16);
    SetValue(mUI.tiVAlign, -1);
    SetValue(mUI.tiHAlign, -1);
    SetValue(mUI.tiTextColor, Qt::white);
    SetValue(mUI.tiLineHeight, 1.0f);
    SetValue(mUI.tiLayer, 0);
    SetValue(mUI.tiRasterWidth, 0);
    SetValue(mUI.tiRasterHeight, 0);
    SetValue(mUI.tiCoordinateSpace, -1);
    SetValue(mUI.tiText, QString(""));
    SetValue(mUI.tiXRotation, 0.0f);
    SetValue(mUI.tiYRotation, 0.0f);
    SetValue(mUI.tiZRotation, 0.0f);
    SetValue(mUI.tiXTranslation, 0.0f);
    SetValue(mUI.tiYTranslation, 0.0f);
    SetValue(mUI.tiZTranslation, 0.0f);
    SetValue(mUI.tiVisible, true);
    SetValue(mUI.tiUnderline, false);
    SetValue(mUI.tiBlink, false);
    SetValue(mUI.tiStatic, false);
    SetValue(mUI.tiBloom, false);
    SetValue(mUI.tiLights, false);
    SetValue(mUI.tiFog, false);
    SetValue(mUI.tiDepthTest, false);
    SetValue(mUI.spnShape, -1);
    SetValue(mUI.spnEnabled, true);
    SetValue(mUI.fxBody, -1);
    SetValue(mUI.fxShape, -1);
    SetValue(mUI.fxPolygon, -1);
    SetValue(mUI.fxFriction, mUI.fxFriction->minimum());
    SetValue(mUI.fxBounciness, mUI.fxBounciness->minimum());
    SetValue(mUI.fxDensity, mUI.fxDensity->minimum());
    SetValue(mUI.fxIsSensor, false);
    SetValue(mUI.mnHCenter, 0.5f);
    SetValue(mUI.mnVCenter, 1.0f);
    SetValue(mUI.nodeMapLayer, 0);
    SetValue(mUI.nodeTileOcclusion, game::TileOcclusion::None);
    SetValue(mUI.splineCoordSpace, game::SplineMoverClass::PathCoordinateSpace::Absolute);
    SetValue(mUI.splineCurveType, game::SplineMoverClass::PathCurveType::CatmullRom);
    SetValue(mUI.splineRotation, game::SplineMoverClass::RotationMode::ApplySplineRotation);
    SetValue(mUI.splineLooping, game::SplineMoverClass::IterationMode::Once);
    SetValue(mUI.splineSpeed, 0.0f);
    SetValue(mUI.splineAcceleration, 0.0f);
    SetValue(mUI.splineFlagEnabled, false);

    SetValue(mUI.tfIntegrator, game::LinearMoverClass::Integrator::Euler);
    SetValue(mUI.tfVelocityX, 0.0f);
    SetValue(mUI.tfVelocityY, 0.0f);
    SetValue(mUI.tfVelocityA, 0.0f);
    SetValue(mUI.tfAccelX, 0.0f);
    SetValue(mUI.tfAccelY, 0.0f);
    SetValue(mUI.tfAccelA, 0.0f);
    SetValue(mUI.tfEnabled, false);
    SetValue(mUI.tfRotate, false);
    SetEnabled(mUI.nodeScrollAreaWidgetContents, false);

    SetEnabled(mUI.actionAddDrawable,    true);
    SetEnabled(mUI.actionAddTextItem,    true);
    SetEnabled(mUI.actionAddRigidBody,   true);
    SetEnabled(mUI.actionAddFixture,     true);
    SetEnabled(mUI.actionAddTilemapNode, true);
    SetEnabled(mUI.actionAddSpatialNode, true);
    SetEnabled(mUI.actionAddLinearMover, true);
    SetEnabled(mUI.actionAddSplineMover, true);
    SetEnabled(mUI.actionAddLight,       true);

    SetVisible(mUI.drawable,    false);
    SetVisible(mUI.textItem,    false);
    SetVisible(mUI.rigidBody,   false);
    SetVisible(mUI.fixture,     false);
    SetVisible(mUI.tilemapNode, false);
    SetVisible(mUI.spatialNode, false);
    SetVisible(mUI.linearMover, false);
    SetVisible(mUI.splineMover, false);
    SetVisible(mUI.basicLight,  false);
    SetVisible(mUI.meshEffect,  false);

    if (auto* node = GetCurrentNode())
    {
        const auto locked = node->TestFlag(game::EntityNodeClass::Flags::LockedInEditor);

        SetEnabled(mUI.nodeProperties, true);
        SetEnabled(mUI.nodeTransform,  !locked);
        SetEnabled(mUI.nodeScrollAreaWidgetContents, true);
        SetEnabled(mUI.btnAddNodeItem, true);

        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        const auto& scale = node->GetScale();
        SetValue(mUI.nodeID, node->GetId());
        SetValue(mUI.nodeName, node->GetName());
        SetValue(mUI.nodeTag, node->GetTag());
        SetValue(mUI.nodeIndex, mState.entity->FindNodeIndex(node));
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeSizeX, size.x);
        SetValue(mUI.nodeSizeY, size.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));
        if (const auto* ptr = base::SafeFind(mComments, node->GetId()))
            SetValue(mUI.nodeComment, *ptr);

        if (const auto* item = node->GetDrawable())
        {
            SetEnabled(mUI.actionAddDrawable, false);
            SetVisible(mUI.drawable, true);
            SetValue(mUI.dsMaterial, ListItemId(item->GetMaterialId()));
            SetValue(mUI.dsDrawable, ListItemId(item->GetDrawableId()));
            SetValue(mUI.dsRenderPass, item->GetRenderPass());
            SetValue(mUI.dsCoordinateSpace, item->GetCoordinateSpace());
            SetValue(mUI.dsLayer, item->GetLayer());
            SetValue(mUI.dsTimeScale, item->GetTimeScale());
            SetValue(mUI.dsVisible, item->TestFlag(game::DrawableItemClass::Flags::VisibleInGame));
            SetValue(mUI.dsUpdateDrawable, item->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable));
            SetValue(mUI.dsUpdateMaterial, item->TestFlag(game::DrawableItemClass::Flags::UpdateMaterial));
            SetValue(mUI.dsRestartDrawable, item->TestFlag(game::DrawableItemClass::Flags::RestartDrawable));
            SetValue(mUI.dsFlipHorizontally, item->TestFlag(game::DrawableItemClass::Flags::FlipHorizontally));
            SetValue(mUI.dsFlipVertically, item->TestFlag(game::DrawableItemClass::Flags::FlipVertically));
            SetValue(mUI.dsBloom, item->TestFlag(game::DrawableItemClass::Flags::PP_EnableBloom));
            SetValue(mUI.dsLights3D, item->TestFlag(game::DrawableItemClass::Flags::EnableLight));
            SetValue(mUI.dsFog3D, item->TestFlag(game::DrawableItemClass::Flags::EnableFog));
            SetValue(mUI.dsDoubleSided, item->TestFlag(game::DrawableItemClass::Flags::DoubleSided));
            SetValue(mUI.dsDepthTest, item->TestFlag(game::DrawableItemClass::Flags::DepthTest));

            const auto& rotator = item->GetRenderRotation();
            const auto [x, y, z] = rotator.GetEulerAngles();
            SetValue(mUI.dsXRotation, x.ToDegrees());
            SetValue(mUI.dsYRotation, y.ToDegrees());
            SetValue(mUI.dsZRotation, z.ToDegrees());

            const auto& translation = item->GetRenderTranslation();
            SetValue(mUI.dsXOffset, translation.x);
            SetValue(mUI.dsYOffset, translation.y);
            SetValue(mUI.dsZOffset, translation.z);
            SetValue(mUI.dsDepth, item->GetDepth());

            const auto& material = mState.workspace->GetResourceById(GetItemId(mUI.dsMaterial));
            const auto& drawable = mState.workspace->GetResourceById(GetItemId(mUI.dsDrawable));
            SetEnabled(mUI.btnEditMaterial, !material.IsPrimitive());
            SetEnabled(mUI.btnEditDrawable, !drawable.IsPrimitive());
            if (drawable.GetType() == app::Resource::Type::Shape)
                mUI.btnEditDrawable->setIcon(QIcon("icons:polygon.png"));
            else if (drawable.GetType() == app::Resource::Type::ParticleSystem)
                mUI.btnEditDrawable->setIcon(QIcon("icons:particle.png"));
        }
        if (const auto* body = node->GetRigidBody())
        {
            SetEnabled(mUI.actionAddRigidBody, false);
            SetVisible(mUI.rigidBody, true);
            SetValue(mUI.rbSimulation, body->GetSimulation());
            SetValue(mUI.rbShape, body->GetCollisionShape());
            SetValue(mUI.rbFriction, body->GetFriction());
            SetValue(mUI.rbRestitution, body->GetRestitution());
            SetValue(mUI.rbAngularDamping, body->GetAngularDamping());
            SetValue(mUI.rbLinearDamping, body->GetLinearDamping());
            SetValue(mUI.rbDensity, body->GetDensity());
            SetValue(mUI.rbIsBullet, body->TestFlag(game::RigidBodyClass::Flags::Bullet));
            SetValue(mUI.rbIsSensor, body->TestFlag(game::RigidBodyClass::Flags::Sensor));
            SetValue(mUI.rbIsEnabled, body->TestFlag(game::RigidBodyClass::Flags::Enabled));
            SetValue(mUI.rbCanSleep, body->TestFlag(game::RigidBodyClass::Flags::CanSleep));
            SetValue(mUI.rbDiscardRotation, body->TestFlag(game::RigidBodyClass::Flags::DiscardRotation));
            if (body->GetCollisionShape() == game::RigidBodyClass::CollisionShape::Polygon)
            {
                SetEnabled(mUI.rbPolygon, true);
                SetValue(mUI.rbPolygon, ListItemId(body->GetPolygonShapeId()));
            }
            else
            {
                SetEnabled(mUI.rbPolygon, false);
                SetValue(mUI.rbPolygon, QString(""));
            }
        }
        if (const auto* text = node->GetTextItem())
        {
            SetEnabled(mUI.actionAddTextItem, false);
            SetVisible(mUI.textItem, true);
            SetValue(mUI.tiFontName, text->GetFontName());
            SetValue(mUI.tiFontSize, text->GetFontSize());
            SetValue(mUI.tiVAlign, text->GetVAlign());
            SetValue(mUI.tiHAlign, text->GetHAlign());
            SetValue(mUI.tiTextColor, text->GetTextColor());
            SetValue(mUI.tiLineHeight, text->GetLineHeight());
            SetValue(mUI.tiLayer, text->GetLayer());
            SetValue(mUI.tiCoordinateSpace, text->GetCoordinateSpace());
            SetValue(mUI.tiRasterWidth, text->GetRasterWidth());
            SetValue(mUI.tiRasterHeight, text->GetRasterHeight());
            SetValue(mUI.tiText, text->GetText());
            SetValue(mUI.tiVisible, text->TestFlag(game::TextItemClass::Flags::VisibleInGame));
            SetValue(mUI.tiUnderline, text->TestFlag(game::TextItemClass::Flags::UnderlineText));
            SetValue(mUI.tiBlink, text->TestFlag(game::TextItemClass::Flags::BlinkText));
            SetValue(mUI.tiStatic, text->TestFlag(game::TextItemClass::Flags::StaticContent));
            SetValue(mUI.tiBloom, text->TestFlag(game::TextItemClass::Flags::PP_EnableBloom));
            SetValue(mUI.tiLights, text->TestFlag(game::TextItemClass::Flags::EnableLight));
            SetValue(mUI.tiFog, text->TestFlag(game::TextItemClass::Flags::EnableFog));
            SetValue(mUI.tiDepthTest, text->TestFlag(game::TextItemClass::Flags::DepthTest));

            const auto& rotator = text->GetRenderRotation();
            const auto [x, y, z] = rotator.GetEulerAngles();
            SetValue(mUI.tiXRotation, x.ToDegrees());
            SetValue(mUI.tiYRotation, y.ToDegrees());
            SetValue(mUI.tiZRotation, z.ToDegrees());

            const auto& translation = text->GetRenderTranslation();
            SetValue(mUI.tiXTranslation, translation.x);
            SetValue(mUI.tiYTranslation, translation.y);
            SetValue(mUI.tiZTranslation, translation.z);
        }
        if (const auto* sp = node->GetSpatialNode())
        {
            SetEnabled(mUI.actionAddSpatialNode, false);
            SetVisible(mUI.spatialNode, true);

            SetValue(mUI.spnShape, sp->GetShape());
            SetValue(mUI.spnEnabled, sp->TestFlag(game::SpatialNodeClass::Flags::Enabled));
        }
        if (const auto* fixture = node->GetFixture())
        {
            SetEnabled(mUI.actionAddFixture, false);
            SetVisible(mUI.fixture, true);

            SetValue(mUI.fxBody, ListItemId(fixture->GetRigidBodyNodeId()));
            SetValue(mUI.fxShape, fixture->GetCollisionShape());
            if (fixture->GetCollisionShape() == game::FixtureClass::CollisionShape::Polygon)
            {
                SetEnabled(mUI.fxPolygon, true);
                SetValue(mUI.fxPolygon, ListItemId(fixture->GetPolygonShapeId()));
            }
            else
            {
                SetEnabled(mUI.fxPolygon, false);
                SetValue(mUI.fxPolygon, -1);
            }
            if (const auto* val = fixture->GetFriction())
                SetValue(mUI.fxFriction, *val);
            if (const auto* val = fixture->GetRestitution())
                SetValue(mUI.fxBounciness, *val);
            if (const auto* val = fixture->GetDensity())
                SetValue(mUI.fxDensity, *val);

            SetValue(mUI.fxIsSensor, fixture->TestFlag(game::FixtureClass::Flags::Sensor));
        }
        if (const auto* map = node->GetMapNode())
        {
            SetEnabled(mUI.actionAddTilemapNode, false);
            SetVisible(mUI.tilemapNode, true);

            const auto& center = map->GetSortPoint();
            SetValue(mUI.mnVCenter, center.y);
            SetValue(mUI.mnHCenter, center.x);
            SetValue(mUI.nodeMapLayer, map->GetMapLayer());
            SetValue(mUI.nodeTileOcclusion, map->GetTileOcclusion());
        }
        if (const auto* mover = node->GetLinearMover())
        {
            SetEnabled(mUI.actionAddLinearMover, false);
            SetVisible(mUI.linearMover, true);

            const auto& accel = mover->GetLinearAcceleration();
            const auto& velo  = mover->GetLinearVelocity();
            SetValue(mUI.tfIntegrator, mover->GetIntegrator());
            SetValue(mUI.tfVelocityX, velo.x);
            SetValue(mUI.tfVelocityY, velo.y);
            SetValue(mUI.tfVelocityA, mover->GetAngularVelocity());
            SetValue(mUI.tfAccelX, accel.x);
            SetValue(mUI.tfAccelY, accel.y);
            SetValue(mUI.tfAccelA, mover->GetAngularAcceleration());
            SetValue(mUI.tfEnabled, mover->IsEnabled());
            SetValue(mUI.tfRotate, mover->RotateToDirection());
        }
        if (auto* mover = node->GetSplineMover())
        {
            SetEnabled(mUI.actionAddSplineMover, false);
            SetVisible(mUI.splineMover, true);
            SetValue(mUI.splineCoordSpace, mover->GetPathCoordinateSpace());
            SetValue(mUI.splineCurveType, mover->GetPathCurveType());
            SetValue(mUI.splineRotation, mover->GetRotationMode());
            SetValue(mUI.splineLooping, mover->GetIterationMode());
            SetValue(mUI.splineSpeed, mover->GetSpeed());
            SetValue(mUI.splineAcceleration, mover->GetAcceleration());
            SetValue(mUI.splineFlagEnabled, mover->IsEnabled());
            mSplineModel->Reset(mover);
        }

        if (const auto* light = node->GetBasicLight())
        {
            SetEnabled(mUI.actionAddLight, false);
            SetVisible(mUI.basicLight, true);

            SetValue(mUI.ltType, light->GetLightType());
            SetValue(mUI.ltAmbient, light->GetAmbientColor());
            SetValue(mUI.ltDiffuse, light->GetDiffuseColor());
            SetValue(mUI.ltSpecular, light->GetSpecularColor());
            SetValue(mUI.ltConstantAttenuation, light->GetConstantAttenuation());
            SetValue(mUI.ltLinearAttenuation, light->GetLinearAttenuation());
            SetValue(mUI.ltQuadraticAttenuation, light->GetQuadraticAttenuation());
            SetValue(mUI.ltTranslation, light->GetTranslation());
            SetValue(mUI.ltDirection, light->GetDirection());
            SetValue(mUI.ltSpotHalfAngle, light->GetSpotHalfAngle());
            SetValue(mUI.ltLayer, light->GetLayer());
            SetValue(mUI.ltEnabled, light->IsEnabled());
        }

        if (const auto* effect = node->GetMeshEffect())
        {
            SetVisible(mUI.meshEffect, true);
            SetValue(mUI.meshEffectType, effect->GetEffectType());
            SetValue(mUI.meshEffectShape, ListItemId(effect->GetEffectShapeId()));
            SetEnabled(mUI.btnResetEffectShape, effect->HasEffectShapeId());
            SetEnabled(mUI.shardIterations, !effect->HasEffectShapeId());
            if (const auto* args = effect->GetMeshExplosionEffectArgs())
            {
                SetValue(mUI.shardIterations, args->mesh_subdivision_count);
                SetValue(mUI.shardLinearVelo, args->shard_linear_speed);
                SetValue(mUI.shardLinearAccel, args->shard_linear_acceleration);
                SetValue(mUI.shardRotVelo, args->shard_rotational_speed);
                SetValue(mUI.shardRotAccel, args->shard_rotational_acceleration);
            }
        }
    }
    else
    {
        mSplineModel->Reset(nullptr);
    }
}

void EntityWidget::DisplayCurrentCameraLocation()
{
    SetValue(mUI.translateX, -mState.camera_offset_x);
    SetValue(mUI.translateY, -mState.camera_offset_y);
}

void EntityWidget::UncheckPlacementActions()
{
    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewIsoscelesTriangle->setChecked(false);
    mUI.actionNewRightTriangle->setChecked(false);
    mUI.actionNewRoundRect->setChecked(false);
    mUI.actionNewTrapezoid->setChecked(false);
    mUI.actionNewParallelogram->setChecked(false);
    mUI.actionNewCapsule->setChecked(false);
    mUI.actionNewSemiCircle->setChecked(false);
    mParticleSystems->menuAction()->setChecked(false);
    mCustomShapes->menuAction()->setChecked(false);
    mUI.actionNewJoint->setChecked(false);

    mUI.actionNewCube->setChecked(false);
    mUI.actionNewCone->setChecked(false);
    mUI.actionNewCylinder->setChecked(false);
    mUI.actionNewPyramid->setChecked(false);
    mUI.actionNewSphere->setChecked(false);

    // this is the wrong place but.. it's convenient
    mUI.widget->SetCursorShape(GfxWidget::CursorShape::ArrowCursor);
}

void EntityWidget::TranslateCamera(float dx, float dy)
{
    mState.camera_offset_x += dx;
    mState.camera_offset_y += dy;
    DisplayCurrentCameraLocation();
}

void EntityWidget::TranslateCurrentNode(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        if (node->TestFlag(game::EntityNodeClass::Flags::LockedInEditor))
        {
            NOTE("Unlock node to apply node transformations.");
            return;
        }

        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
        SetValue(mUI.nodeTranslateX, pos.x);
        SetValue(mUI.nodeTranslateY, pos.y);
        RealizeEntityChange(mState.entity);
    }
}

void EntityWidget::UpdateCurrentNodeProperties()
{
    auto* node = GetCurrentNode();
    if (node == nullptr)
        return;

    if (!node->TestFlag(game::EntityNodeClass::Flags::LockedInEditor))
    {
        glm::vec2 size, scale, translation;
        size.x = GetValue(mUI.nodeSizeX);
        size.y = GetValue(mUI.nodeSizeY);
        scale.x = GetValue(mUI.nodeScaleX);
        scale.y = GetValue(mUI.nodeScaleY);
        translation.x = GetValue(mUI.nodeTranslateX);
        translation.y = GetValue(mUI.nodeTranslateY);
        node->SetSize(size);
        node->SetScale(scale);
        node->SetTranslation(translation);
        node->SetRotation(qDegreesToRadians((float)GetValue(mUI.nodeRotation)));
    }

    if (auto* item = node->GetDrawable())
    {
        item->SetDrawableId(GetItemId(mUI.dsDrawable));
        item->SetMaterialId(GetItemId(mUI.dsMaterial));
        item->SetTimeScale(GetValue(mUI.dsTimeScale));
        item->SetLayer(GetValue(mUI.dsLayer));
        item->SetRenderPass(GetValue(mUI.dsRenderPass));
        item->SetCoordinateSpace(GetValue(mUI.dsCoordinateSpace));
        item->SetDepth(GetValue(mUI.dsDepth));

        game::Rotator rotator = base::Rotator::FromEulerXYZ(
                base::FDegrees((float)GetValue(mUI.dsXRotation)),
                base::FDegrees((float)GetValue(mUI.dsYRotation)),
                base::FDegrees((float)GetValue(mUI.dsZRotation)));
        item->SetRenderRotation(rotator);

        glm::vec3 render_translation;
        render_translation.x = GetValue(mUI.dsXOffset);
        render_translation.y = GetValue(mUI.dsYOffset);
        render_translation.z = GetValue(mUI.dsZOffset);
        item->SetRenderTranslation(render_translation);

        item->SetFlag(game::DrawableItemClass::Flags::VisibleInGame, GetValue(mUI.dsVisible));
        item->SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, GetValue(mUI.dsUpdateDrawable));
        item->SetFlag(game::DrawableItemClass::Flags::UpdateMaterial, GetValue(mUI.dsUpdateMaterial));
        item->SetFlag(game::DrawableItemClass::Flags::RestartDrawable, GetValue(mUI.dsRestartDrawable));
        item->SetFlag(game::DrawableItemClass::Flags::FlipHorizontally, GetValue(mUI.dsFlipHorizontally));
        item->SetFlag(game::DrawableItemClass::Flags::FlipVertically, GetValue(mUI.dsFlipVertically));
        item->SetFlag(game::DrawableItemClass::Flags::PP_EnableBloom, GetValue(mUI.dsBloom));
        item->SetFlag(game::DrawableItemClass::Flags::EnableLight, GetValue(mUI.dsLights3D));
        item->SetFlag(game::DrawableItemClass::Flags::EnableFog, GetValue(mUI.dsFog3D));
        item->SetFlag(game::DrawableItemClass::Flags::DoubleSided, GetValue(mUI.dsDoubleSided));
        item->SetFlag(game::DrawableItemClass::Flags::DepthTest, GetValue(mUI.dsDepthTest));
    }

    if (auto* body = node->GetRigidBody())
    {
        body->SetPolygonShapeId(GetItemId(mUI.rbPolygon));
        body->SetSimulation(GetValue(mUI.rbSimulation));
        body->SetCollisionShape(GetValue(mUI.rbShape));
        body->SetFriction(GetValue(mUI.rbFriction));
        body->SetRestitution(GetValue(mUI.rbRestitution));
        body->SetAngularDamping(GetValue(mUI.rbAngularDamping));
        body->SetLinearDamping(GetValue(mUI.rbLinearDamping));
        body->SetDensity(GetValue(mUI.rbDensity));

        // flags
        body->SetFlag(game::RigidBodyClass::Flags::Bullet, GetValue(mUI.rbIsBullet));
        body->SetFlag(game::RigidBodyClass::Flags::Sensor, GetValue(mUI.rbIsSensor));
        body->SetFlag(game::RigidBodyClass::Flags::Enabled, GetValue(mUI.rbIsEnabled));
        body->SetFlag(game::RigidBodyClass::Flags::CanSleep, GetValue(mUI.rbCanSleep));
        body->SetFlag(game::RigidBodyClass::Flags::DiscardRotation, GetValue(mUI.rbDiscardRotation));
    }

    if (auto* text = node->GetTextItem())
    {
        game::Rotator rotator = base::Rotator::FromEulerXYZ(
                base::FDegrees((float)GetValue(mUI.tiXRotation)),
                base::FDegrees((float)GetValue(mUI.tiYRotation)),
                base::FDegrees((float)GetValue(mUI.tiZRotation)));
        text->SetRenderRotation(rotator);

        glm::vec3 render_translation = {0.0f, 0.0f, 0.0f};
        render_translation.x = GetValue(mUI.tiXTranslation);
        render_translation.y = GetValue(mUI.tiYTranslation);
        render_translation.z = GetValue(mUI.tiZTranslation);
        text->SetRenderTranslation(render_translation);

        text->SetFontName(GetValue(mUI.tiFontName));
        text->SetFontSize(GetValue(mUI.tiFontSize));
        text->SetAlign((game::TextItemClass::VerticalTextAlign)GetValue(mUI.tiVAlign));
        text->SetAlign((game::TextItemClass::HorizontalTextAlign)GetValue(mUI.tiHAlign));
        text->SetTextColor(GetValue(mUI.tiTextColor));
        text->SetLineHeight(GetValue(mUI.tiLineHeight));
        text->SetText(GetValue(mUI.tiText));
        text->SetLayer(GetValue(mUI.tiLayer));
        text->SetCoordinateSpace(GetValue(mUI.tiCoordinateSpace));
        text->SetRasterWidth(GetValue(mUI.tiRasterWidth));
        text->SetRasterHeight(GetValue(mUI.tiRasterHeight));
        // flags
        text->SetFlag(game::TextItemClass::Flags::VisibleInGame, GetValue(mUI.tiVisible));
        text->SetFlag(game::TextItemClass::Flags::UnderlineText, GetValue(mUI.tiUnderline));
        text->SetFlag(game::TextItemClass::Flags::BlinkText, GetValue(mUI.tiBlink));
        text->SetFlag(game::TextItemClass::Flags::StaticContent, GetValue(mUI.tiStatic));
        text->SetFlag(game::TextItemClass::Flags::PP_EnableBloom, GetValue(mUI.tiBloom));
        text->SetFlag(game::TextItemClass::Flags::EnableLight, GetValue(mUI.tiLights));
        text->SetFlag(game::TextItemClass::Flags::EnableFog, GetValue(mUI.tiFog));
        text->SetFlag(game::TextItemClass::Flags::DepthTest, GetValue(mUI.tiDepthTest));
    }
    if (auto* fixture = node->GetFixture())
    {
        fixture->SetRigidBodyNodeId(GetItemId(mUI.fxBody));
        fixture->SetPolygonShapeId(GetItemId(mUI.fxPolygon));
        fixture->SetCollisionShape(GetValue(mUI.fxShape));
        const float friction   = GetValue(mUI.fxFriction);
        const float density    = GetValue(mUI.fxDensity);
        const float bounciness = GetValue(mUI.fxBounciness);
        if (friction >= 0.0f)
            fixture->SetFriction(friction);
        else fixture->ResetFriction();

        if (density >= 0.0f)
            fixture->SetDensity(density);
        else fixture->ResetDensity();

        if (bounciness >= 0.0f)
            fixture->SetRestitution(bounciness);
        else fixture->ResetRestitution();

        fixture->SetFlag(game::FixtureClass::Flags::Sensor, GetValue(mUI.fxIsSensor));
    }

    if (auto* sp = node->GetSpatialNode())
    {
        sp->SetShape(GetValue(mUI.spnShape));
        sp->SetFlag(game::SpatialNodeClass::Flags::Enabled, GetValue(mUI.spnEnabled));
    }

    if (auto* map = node->GetMapNode())
    {
        glm::vec2 center;
        center.x = GetValue(mUI.mnHCenter);
        center.y = GetValue(mUI.mnVCenter);
        map->SetMapSortPoint(center);
        map->SetMapLayer(GetValue(mUI.nodeMapLayer));
        map->SetTileOcclusion(GetValue(mUI.nodeTileOcclusion));
    }

    if (auto* mover = node->GetLinearMover())
    {
        glm::vec2 velocity;
        glm::vec2 acceleration;
        velocity.x = GetValue(mUI.tfVelocityX);
        velocity.y = GetValue(mUI.tfVelocityY);
        acceleration.x = GetValue(mUI.tfAccelX);
        acceleration.y = GetValue(mUI.tfAccelY);
        mover->SetIntegrator(GetValue(mUI.tfIntegrator));
        mover->SetLinearAcceleration(acceleration);
        mover->SetLinearVelocity(velocity);
        mover->SetAngularVelocity(GetValue(mUI.tfVelocityA));
        mover->SetAngularAcceleration(GetValue(mUI.tfAccelA));
        mover->SetFlag(game::LinearMoverClass::Flags::Enabled, GetValue(mUI.tfEnabled));
        mover->SetFlag(game::LinearMoverClass::Flags::RotateToDirection, GetValue(mUI.tfRotate));
    }

    if (auto* mover = node->GetSplineMover())
    {
        mover->SetPathCoordinateSpace(GetValue(mUI.splineCoordSpace));
        mover->SetPathCurveType(GetValue(mUI.splineCurveType));
        mover->SetRotationMode(GetValue(mUI.splineRotation));
        mover->SetIterationMode(GetValue(mUI.splineLooping));
        mover->SetSpeed(GetValue(mUI.splineSpeed));
        mover->SetAcceleration(GetValue(mUI.splineAcceleration));
        mover->SetFlag(game::SplineMoverClass::Flags::Enabled, GetValue(mUI.splineFlagEnabled));
    }

    if (auto* light = node->GetBasicLight())
    {
        const game::FDegrees spot_half_angle = GetValue(mUI.ltSpotHalfAngle);
        light->SetLightType(GetValue(mUI.ltType));
        light->SetAmbientColor(GetValue(mUI.ltAmbient));
        light->SetDiffuseColor(GetValue(mUI.ltDiffuse));
        light->SetSpecularColor(GetValue(mUI.ltSpecular));
        light->SetLinearAttenuation(GetValue(mUI.ltLinearAttenuation));
        light->SetConstantAttenuation(GetValue(mUI.ltConstantAttenuation));
        light->SetQuadraticAttenuation(GetValue(mUI.ltQuadraticAttenuation));
        light->SetTranslation(GetValue(mUI.ltTranslation));
        light->SetDirection(GetValue(mUI.ltDirection));
        light->SetSpotHalfAngle(spot_half_angle);
        light->SetLayer(GetValue(mUI.ltLayer));
        light->Enable(GetValue(mUI.ltEnabled));
    }

    if (auto* effect = node->GetMeshEffect())
    {
        effect->SetEffectType(GetValue(mUI.meshEffectType));
        effect->SetEffectShapeId(GetItemId(mUI.meshEffectShape));
        const auto type = effect->GetEffectType();
        if (type == game::MeshEffectClass::EffectType::MeshExplosion)
        {
            game::MeshEffectClass::MeshExplosionEffectArgs args;
            args.mesh_subdivision_count = GetValue(mUI.shardIterations);
            args.shard_linear_speed = GetValue(mUI.shardLinearVelo);
            args.shard_linear_acceleration = GetValue(mUI.shardLinearAccel);
            args.shard_rotational_speed = GetValue(mUI.shardRotVelo);
            args.shard_rotational_acceleration = GetValue(mUI.shardRotAccel);
            effect->SetEffectArgs(args);
        }
    }

    RealizeEntityChange(mState.entity);
}

void EntityWidget::RebuildMenus()
{
    // rebuild the drawable menus for custom shapes
    // and particle systems.
    mParticleSystems->clear();
    mCustomShapes->clear();
    for (size_t i = 0; i < mState.workspace->GetNumResources(); ++i)
    {
        const auto& resource = mState.workspace->GetResource(i);
        const auto& name = resource.GetName();
        const auto& id   = resource.GetId();
        if (resource.GetType() == app::Resource::Type::ParticleSystem)
        {
            QAction* action = mParticleSystems->addAction(name);
            action->setData(id);
            connect(action, &QAction::triggered, this, &EntityWidget::PlaceNewParticleSystem);
        }
        else if (resource.GetType() == app::Resource::Type::Shape)
        {
            QAction* action = mCustomShapes->addAction(name);
            action->setData(id);
            connect(action, &QAction::triggered,this, &EntityWidget::PlaceNewCustomShape);
        }
    }
    mParticleSystems->addSeparator();
    mParticleSystems->addAction(mUI.actionAddPresetParticle);
}

void EntityWidget::RebuildCombos()
{
    SetList(mUI.dsMaterial, mState.workspace->ListAllMaterials());
    SetList(mUI.dsDrawable, mState.workspace->ListAllDrawables());

    std::vector<ResourceListItem> polygons;
    std::vector<ResourceListItem> scripts;
    std::vector<ResourceListItem> effect_polygons;

    // for the rigid body we need to list the polygonal (custom) shape
    // objects. (note that it's actually possible that these would be concave
    // but this case isn't currently supported)
    for (size_t i=0; i<mState.workspace->GetNumUserDefinedResources(); ++i)
    {
        const auto& res = mState.workspace->GetUserDefinedResource(i);
        ResourceListItem pair;
        pair.name = res.GetName();
        pair.id   = res.GetId();
        if (res.GetType() == app::Resource::Type::Shape)
        {
            polygons.push_back(pair);

            const gfx::PolygonMeshClass* polygon_class = nullptr;
            res.GetContent(&polygon_class);
            ASSERT(polygon_class);
            if (polygon_class->GetMeshType() == gfx::PolygonMeshClass::MeshType::Simple2DShardEffectMesh)
                effect_polygons.push_back(pair);
        }
        else if (res.GetType() == app::Resource::Type::Script)
        {
            scripts.push_back(pair);
        }
    }
    SetList(mUI.rbPolygon, polygons);
    SetList(mUI.fxPolygon, polygons);
    SetList(mUI.scriptFile, scripts);
    SetList(mUI.meshEffectShape, effect_polygons);
}

void EntityWidget::RebuildCombosInternal()
{
    std::vector<ResourceListItem> bodies;

    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        if (auto* body = node.GetRigidBody())
        {
            ResourceListItem pair;
            pair.name = node.GetName();
            pair.id   = node.GetId();
            bodies.push_back(pair);
        }
    }
    SetList(mUI.fxBody, bodies);
}

void EntityWidget::SelectTile()
{
    auto* node = GetCurrentNode();
    if (!node)
        return;

    auto* drawable = node->GetDrawable();
    if (!drawable)
        return;

    const auto& material = mState.workspace->FindMaterialClassById(drawable->GetMaterialId());
    if (!material || material->GetType() != gfx::MaterialClass::Type::Tilemap)
        return;

    DlgTileChooser dlg(this, material);
    if (const auto* ptr = drawable->GetMaterialParamValue<float>("kTileIndex"))
        dlg.SetTileIndex(static_cast<unsigned>(*ptr));

    if (dlg.exec() == QDialog::Accepted)
        drawable->SetMaterialParam("kTileIndex", static_cast<float>(dlg.GetTileIndex()));

    QTimer::singleShot(100, this, [this]() {
        //qApp->focusWidget()->clearFocus();
        //mUI.widget->raise();
        mUI.widget->activateWindow();
        mUI.widget->setFocus();
    });
}

void EntityWidget::UpdateGizmos()
{
    SetValue(mUI.actionSelectObject, mTransformGizmo == TransformGizmo3D::None);
    SetValue(mUI.actionRotateObject, mTransformGizmo == TransformGizmo3D::Rotate);
    SetValue(mUI.actionTranslateObject, mTransformGizmo == TransformGizmo3D::Translate);
}

bool EntityWidget::CanApplyGizmo() const
{
    if (const auto* node = GetCurrentNode())
    {
        if (node->HasDrawable() || node->HasTextItem() || node->HasBasicLight())
            return true;
    }
    return false;
}


void EntityWidget::UpdateDeletedResourceReferences()
{
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        if (auto* draw = node.GetDrawable())
        {
            const auto drawable = draw->GetDrawableId();
            const auto material = draw->GetMaterialId();
            if (!mState.workspace->IsValidMaterial(material))
            {
                WARN("Entity node '%1' uses material which is no longer available." , node.GetName());
                draw->ResetMaterial();
                draw->SetMaterialId("_checkerboard");
            }
            if (!mState.workspace->IsValidDrawable(drawable))
            {
                WARN("Entity node '%1' uses drawable which is no longer available." , node.GetName());
                draw->ResetDrawable();
                draw->SetDrawableId("_rect");
            }
        }
        if (auto* body = node.GetRigidBody())
        {
            if (body->GetCollisionShape() == game::RigidBodyClass::CollisionShape::Polygon)
            {
                if (!mState.workspace->IsValidDrawable(body->GetPolygonShapeId()))
                {
                    WARN("Entity node '%1' uses rigid body shape which is no longer available.", node.GetName());
                    body->ResetPolygonShapeId();
                    body->SetCollisionShape(game::RigidBodyClass::CollisionShape::Box);
                }
            }
            else
            {
                // clean away this stale data
                body->ResetPolygonShapeId();
            }
        }
        if (auto* fixture = node.GetFixture())
        {
            if (fixture->GetCollisionShape() == game::FixtureClass::CollisionShape::Polygon)
            {
                if (!mState.workspace->IsValidDrawable(fixture->GetPolygonShapeId()))
                {
                    WARN("Entity node '%1' fixture uses rigid body shape which is no longer available.", node.GetName());
                    fixture->ResetPolygonShapeId();
                    fixture->SetCollisionShape(game::RigidBodyClass::CollisionShape::Box);
                }
            }
            else
            {
                // clean away stale data.
                fixture->ResetPolygonShapeId();
            }
        }
        if (auto* effect = node.GetMeshEffect())
        {
            if (effect->HasEffectShapeId())
            {
                const auto& effect_shape_id = effect->GetEffectShapeId();
                if (!mState.workspace->IsValidDrawable(effect_shape_id))
                {
                    WARN("Entity node '%1' mesh effect uses an effect shape which is no longer available.", node.GetName());
                    effect->ResetEffectShapeId();
                }
            }
        }
    }

    if (mState.entity->HasScriptFile())
    {
        const auto& scriptId = mState.entity->GetScriptFileId();
        if (!mState.workspace->IsValidScript(scriptId))
        {
            WARN("Entity '%1' script is no longer available.", mState.entity->GetName());
            mState.entity->ResetScriptFile();
            SetEnabled(mUI.btnEditScript, false);
        }
    }
    RealizeEntityChange(mState.entity);
}

game::EntityNodeClass* EntityWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityNodeClass*>(item->GetUserData());
}

const game::EntityNodeClass* EntityWidget::GetCurrentNode() const
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityNodeClass*>(item->GetUserData());
}

size_t EntityWidget::ComputeHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mState.entity->GetHash());
    // include the track properties.
    for (const auto& [key, props] : mTrackProperties)
    {
        hash = base::hash_combine(hash, app::FromUtf8(key));
        for (const auto& value : props)
        {
            hash = base::hash_combine(hash, app::VariantHash(value));
        }
    }
    // include the node specific comments
    for (const auto& [node, comment] : mComments)
    {
        hash = base::hash_combine(hash, node);
        hash = base::hash_combine(hash, app::ToUtf8(comment));
    }
    return hash;
}

glm::vec2 EntityWidget::MapMouseCursorToWorld() const
{
    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    const auto projection = (game::SceneProjection)GetValue(mUI.cmbSceneProjection);
    return MapWindowCoordinateToWorld(mUI, mState, mickey, projection);
}

QString GenerateEntityScriptSource(QString entity)
{
    entity = app::GenerateScriptVarName(entity, "entity");

QString source(R"(
--
-- Entity '%1' script.
--
-- This script will be called for every instance of '%1' in the scene
-- during gameplay.
-- You're free to delete functions you don't need.
--

-- Called once when the game play begins for the entity in the scene.
function BeginPlay(%1, scene, map)
end

-- Called once when the game play ends for the entity in the scene.
function EndPlay(%1, scene, map)
end

-- Called on every low frequency game tick. The tick frequency is
-- determined in the project settings. If you want to perform animation
-- such as move your game objects more smoothly then Update is the place
-- to do it. This function can be used to do thing such as evaluate AI or
-- path finding etc.
function Tick(%1, game_time, dt)
end

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
-- allocator is an instance of game.EntityNodeAllocator that provides
-- the storage for the entity nodes. Keep in mind that this contains
-- *all* the nodes of any specific entity type. So the combination of
-- all the nodes across all entity instances 'klass' type.
-- Any component for any given node (at some index) may be nil so you
-- need to remember to check for nils before accessing.
function UpdateNodes(allocator, game_time, dt, klass)
end

-- Called on every iteration of the game loop. game_time is the current
-- game time so far in seconds not including the next time step dt.
function Update(%1, game_time, dt)
end

-- Called on every iteration of the game loop game after *all* entities
-- in the scene have been updated. This means that all objects are in their
-- final places and it's possible to do things such as query scene spatial
-- nodes for finding interesting objects in any particular location.
function PostUpdate(%1, game_time)
end

-- Called on collision events with other objects based on the information
-- from the physics engine. You can only get these events  when your entity
-- node(s) have rigid bodies and are colliding with other rigid bodies. The
-- contact can exist over multiple time steps depending on the type of bodies etc.
-- Node is this entity's entity node with rigid body that collided with the
-- other entity's other_node's rigid body.
function OnBeginContact(%1, node, other_entity, other_node)
end

-- Similar to OnBeginContact except this happens when the contact ends.
function OnEndContact(%1, node, other_entity, other_node)
end

-- Called on key down events. This is only called when the entity has enabled
-- the keyboard input processing to take place. You can find this setting under
-- 'Script callbacks' in the entity editor. Symbol is one of the virtual key
-- symbols rom the wdk.Keys table and modifier bits is the bitwise combination
-- of control keys (Ctrl, Shift, etc) at the time of the key event.
-- The modifier_bits are expressed as an object of wdk.KeyBitSet.
--
-- Note that because some platforms post repeated events when a key is
-- continuously held you can get this event multiple times without getting
-- the corresponding key up!
function OnKeyDown(%1, symbol, modifier_bits)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(%1, symbol, modifier_bits)
end

-- Called on mouse button press events. This is only called when the entity
-- has enabled the mouse input processing to take place. You can find this
-- setting under 'Script callbacks' in the entity editor.
-- Mouse argument is of type game.MouseEvent and provides an aggregate of
-- information about the event. You can find more details about this type in
-- the Lua API doc.
function OnMousePress(%1, mouse)
end

-- Called on mouse button release events. See OnMousePress for more details.
function OnMouseRelease(%1, mouse)
end

-- Called on mouse move events. See OnMousePress for more details.
function OnMouseMove(%1, mouse)
end

-- Called on game events. Game events are broad-casted to all entities in
-- the scene.  GameEvents are useful when there's an unknown number of
-- entities possibly interested in some game event. Use Game:PostEvent to
-- post a new game event. Each entity will then receive the same event object
-- in this callback and can proceed to process the information.
function OnGameEvent(%1, event)
end

-- Called on animation finished events, i.e. when this entity has finished
-- playing the animation in question.
function OnAnimationFinished(%1, animation)
end

-- Called on timer events. Timers are set on an Entity by calling SetTimer.
-- When the timer expires this callback is then invoked. Timer is then the
-- name of the timer (same as in SetTimer) that fired and jitter defines
-- the difference to ideal time when the timer should have fired. In general
-- entity timers are limited in their resolution to game update resolution.
-- In other words if the game updates at 60 Hz the timer frequency is then
-- 1/60 seconds. If jitter is positive it means the timer is firing early
-- and a negative value indicates the timer fired late.
function OnTimer(%1, timer, jitter)
end

-- Called on posted entity events. Events can be posted on particular entities
-- by calling entity:PostEvent. Unlike game.GameEvents game.EntityEvent are
-- entity specific and only ever delivered to a single entity (the receiver).
function OnEvent(%1, event)
end
    )");
    return source.replace("%1", entity);
}

QString GenerateAnimatorScriptSource()
{
    static const QString source = R"(
--
-- Entity state controller script.
--
-- This script will be called for every entity controller instance that has
-- this particular script assigned. This script allows you to write the logic
-- for performing some particular actions when entering/leaving entity states
-- and when transitioning from one state to another. Good examples are changing
-- the material, drawable states etc. to visually indicate what your character
-- is currently doing.
--
-- You're free to delete functions you don't need.
--

-- Called once when the controller is first created.
-- This is the place where you can set the initial entity and controller state
-- to a known/desired first state.
function Init(controller, entity)

end


-- Called once when the entity enters a new state at the end of a transition.
function EnterState(controller, state, entity)

end

-- Called once when the entity is leaving a state at the start of a transition.
function LeaveState(controller, state, entity)

end

-- Called continuously on the current state.
-- This is the place where you can realize changes to the current input when in
-- some particular state. For example check the current entity velocity or
-- direction to determine which sprite animation to play or which way the
-- character on screen should be looking.
function UpdateState(controller, state, time, dt, entity)

end

-- Evaluate the condition to trigger a transition from one state to another.
-- Return true to take the transition or false to reject it.
--
-- Only a single transition can ever be progress at any given time. If the
-- state has possible transitions to multiple states then whichever state
-- transition evaluation returns true first will be taken and the other
-- transitions will not be considered.
--
-- For example if your state chart has states 'Idle', 'Walk" and 'Run'
-- and 'Idle' can transition to either 'Walk' or 'Run', if the evaluation
-- of 'Idle to Walk' returns true then 'Idle to Run' is never considered.
-- The order in which the possible transitions are evaluated is unspecified.
--
-- This is is controlled by the state evaluation mode in the controller
-- settings. Only when the mode is "Evaluate Continuously' will this be called.
-- Otherwise call TriggerTransition in order to trigger evaluation.
--
function EvalTransition(controller, from, to, entity)
    return false
end

-- Called once when a transition is started from one state to another.
function StartTransition(controller, from, to, duration, entity)

end

-- Called once when the transition from one state to another is finished.
function FinishTransition(controller, from, to, entity)

end

-- Called continuously on a transition while it's in progress.
function UpdateTransition(controller, from, to, duration, time, dt, entity)

end

-- Called on key down events. This is only called when the controller
-- has enabled the keyboard input processing to take place.
--
-- Symbol is one of the virtual key symbols rom the wdk.Keys table and
-- modifier bits is the bitwise combination of control keys (Ctrl, Shift, etc)
-- at the time of the key event. The modifier_bits are expressed as an object
-- of wdk.KeyBitSet.
--
-- Note that because some platforms post repeated events when a key is
-- continuously held you can get this event multiple times without getting
-- the corresponding key up!
function OnKeyDown(controller, symbol, modifier_bits, entity)
end

-- Called on key up events. See OnKeyDown for more details.
function OnKeyUp(controller, symbol, modifier_bits, entity)
end

)";
    return source;
}

} // bui
