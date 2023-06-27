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
#  include <QAbstractTableModel>
#include "warnpop.h"

#include "base/math.h"
#include "editor/app/workspace.h"
#include "editor/gui/dlgentity.h"
#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/utility.h"
#include "editor/app/utility.h"

namespace gui
{
class DlgEntity::ScriptVarModel : public QAbstractTableModel
{
public:
    ScriptVarModel(const app::Workspace* workspace,
            const game::EntityClass& entity, game::EntityPlacement& node)
      : mWorkspace(workspace)
      , mEntity(entity)
      , mNode(node)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto row = index.row();
        const auto col = index.column();
        const auto& var = mEntity.GetScriptVar(row);
        if (role == Qt::DisplayRole)
        {
            if (col == 0) return app::toString(var.GetName());
            else if (col == 1) return app::toString(var.IsReadOnly());
            else if (col == 2)
            {
                const auto* val = mNode.FindScriptVarValueById(var.GetId());
                if (val == nullptr)
                    return "Class Default";
                return GetScriptVarData(*val);
            }
            else BUG("Unknown script variable data index.");
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Name";
            else if (section == 1) return "ReadOnly";
            else if (section == 2) return "Value";
            else BUG("Unknown script variable data index.");
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    { return static_cast<int>(mEntity.GetNumScriptVars()); }
    virtual int columnCount(const QModelIndex&) const override
    { return 3; }

    void SetValue(size_t row, const game::EntityPlacement::ScriptVarValue& value)
    {
        ASSERT(mEntity.FindScriptVarById(value.id));

        mNode.SetScriptVarValue(value);
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void SetValue(size_t row, game::EntityPlacement::ScriptVarValue&& value)
    {
        ASSERT(mEntity.FindScriptVarById(value.id));

        mNode.SetScriptVarValue(std::move(value));
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void DeleteValue(size_t row)
    {
        const auto& var = mEntity.GetScriptVar(row);
        mNode.DeleteScriptVarValueById(var.GetId());
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void Reset()
    {
        beginResetModel();
        endResetModel();
    }

private:
    QVariant GetScriptVarData(const game::EntityPlacement::ScriptVarValue& value) const
    {
        switch (game::ScriptVar::GetTypeFromVariant(value.value))
        {
            case game::ScriptVar::Type::Boolean: {
                const auto& array = std::get<std::vector<bool>>(value.value);
                if (array.size() == 1)
                    return array[0];
                return QString("[0]=%1 ...").arg(array[0]);
            }
            break;
            case game::ScriptVar::Type::String: {
                const auto& array = std::get<std::vector<std::string>>(value.value);
                if (array.size() == 1)
                    return app::FromUtf8(array[0]);
                return QString("[0]='%1' ...").arg(app::FromUtf8(array[0]));
            }
            break;
            case game::ScriptVar::Type::Float: {
                const auto& array = std::get<std::vector<float>>(value.value);
                if (array.size() == 1)
                    return QString::number(array[0], 'f', 2);
                return QString("[0]=%1 ...").arg(QString::number(array[0], 'f', 2));
            }
            break;
            case game::ScriptVar::Type::Integer: {
                const auto& array = std::get<std::vector<int>>(value.value);
                if (array.size() == 1)
                    return array[0];
                return QString("[0]=%1 ...").arg(array[0]);
            }
            break;
            case game::ScriptVar::Type::Vec2: {
                const auto& array = std::get<std::vector<glm::vec2>>(value.value);
                const auto& vec = array[0];
                if (array.size() == 1)
                    return QString("%1,%2")
                            .arg(QString::number(vec.x, 'f', 2))
                            .arg(QString::number(vec.y, 'f', 2));

                return QString("[0]=%1,%2 ...")
                        .arg(QString::number(vec.x, 'f', 2))
                        .arg(QString::number(vec.y, 'f', 2));
            }
            break;
            case game::ScriptVar::Type::EntityReference: {
                const auto& array = std::get<std::vector<game::ScriptVar::EntityReference>>(value.value);
                if (array.size() == 1)
                    return "Nil";
                return "[0]=Nil ...";
            }
            break;
            case game::ScriptVar::Type::EntityNodeReference: {
                const auto& array = std::get<std::vector<game::ScriptVar::EntityNodeReference>>(value.value);
                const auto& id = array[0].id;
                QString name = "Nil";
                if (const auto* node = mEntity.FindNodeById(id))
                    name = app::FromUtf8(node->GetName());
                if (array.size() == 1)
                    return name;
                return QString("[0]=%1 ...").arg(name);
            }
            case game::ScriptVar::Type::MaterialReference: {
                const auto& array = std::get<std::vector<game::ScriptVar::MaterialReference>>(value.value);
                const auto& id = array[0].id;
                QString name = "Nil";
                if (const auto& material = mWorkspace->FindMaterialClassById(id))
                    name = app::FromUtf8(material->GetName());
                if (array.size() == 1)
                    return name;
                return QString("[0]=%1 ...").arg(name);
            }
        }
        BUG("Unknown ScriptVar type.");
        return QVariant();
    }
private:
    const app::Workspace* mWorkspace = nullptr;
    const game::EntityClass& mEntity;
    game::EntityPlacement& mNode;
};

DlgEntity::DlgEntity(QWidget* parent, const app::Workspace* workspace,
                     const game::EntityClass& klass, game::EntityPlacement& node)
    : QDialog(parent)
    , mWorkspace(workspace)
    , mEntityClass(klass)
    , mNodeClass(node)
{
    mScriptVars.reset(new ScriptVarModel(workspace, klass, node));

    mUI.setupUi(this);
    mUI.tableView->setModel(mScriptVars.get());

    SetValue(mUI.grpEntity, app::toString("Entity instance - %1", node.GetName()));
    SetValue(mUI.entityLifetime, 0.0f);
    SetValue(mUI.chkKillAtLifetime, Qt::PartiallyChecked);
    SetValue(mUI.chkKillAtBoundary, Qt::PartiallyChecked);
    SetValue(mUI.chkUpdateEntity, Qt::PartiallyChecked);
    SetValue(mUI.chkTickEntity, Qt::PartiallyChecked);
    SetValue(mUI.chkKeyEvents, Qt::PartiallyChecked);
    SetValue(mUI.chkMouseEvents, Qt::PartiallyChecked);
    SetEnabled(mUI.btnResetVar, false);
    SetEnabled(mUI.btnEditVar, false);

    if (const auto* tag = node.GetTag())
    {
        SetValue(mUI.entityTag, *tag);
        // the tag string could be an empty string which means the
        // placeholder text must be removed in order to indicate on the UI
        // correctly that the placeholder text is set, but it's set to an
        // emtpy string. Otherwise, the UI will indicate "Class Default".
        SetPlaceholderText(mUI.entityTag, QString(""));
    }
    else
    {
        SetValue(mUI.entityTag, QString(""));
        SetPlaceholderText(mUI.entityTag, QString("Class Default"));
    }

    std::vector<ResourceListItem> tracks;
    for (size_t i=0; i< klass.GetNumAnimations(); ++i)
    {
        const auto& track = klass.GetAnimation(i);
        ResourceListItem item;
        item.name = app::FromUtf8(track.GetName());
        item.id   = app::FromUtf8(track.GetId());
        tracks.push_back(std::move(item));
    }
    SetList(mUI.idleAnimation, tracks);
    SetValue(mUI.idleAnimation, ListItemId(node.GetIdleAnimationId()));
    if (mNodeClass.HasLifetimeSetting())
        SetValue(mUI.entityLifetime, mNodeClass.GetLifetime());

    GetFlag(game::EntityPlacement::Flags::KillAtLifetime, mUI.chkKillAtLifetime);
    GetFlag(game::EntityPlacement::Flags::KillAtBoundary, mUI.chkKillAtBoundary);
    GetFlag(game::EntityPlacement::Flags::UpdateEntity, mUI.chkUpdateEntity);
    GetFlag(game::EntityPlacement::Flags::TickEntity, mUI.chkTickEntity);
    GetFlag(game::EntityPlacement::Flags::WantsKeyEvents, mUI.chkKeyEvents);
    GetFlag(game::EntityPlacement::Flags::WantsMouseEvents, mUI.chkMouseEvents);

    connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &DlgEntity::ScriptVariableSelectionChanged);
}

DlgEntity::~DlgEntity() = default;

void DlgEntity::on_btnAccept_clicked()
{
    mNodeClass.SetIdleAnimationId(GetItemId(mUI.idleAnimation));

    const float lifetime = GetValue(mUI.entityLifetime);
    if (lifetime != 0.0)
    {
        mNodeClass.SetLifetime(lifetime);
        mNodeClass.SetFlag(game::EntityPlacement::Flags::LimitLifetime, true);
    }
    else
    {
        mNodeClass.ResetLifetime();
        mNodeClass.SetFlag(game::EntityPlacement::Flags::LimitLifetime, false);
    }

    SetFlag(game::EntityPlacement::Flags::KillAtLifetime, mUI.chkKillAtLifetime);
    SetFlag(game::EntityPlacement::Flags::KillAtBoundary, mUI.chkKillAtBoundary);
    SetFlag(game::EntityPlacement::Flags::UpdateEntity, mUI.chkUpdateEntity);
    SetFlag(game::EntityPlacement::Flags::TickEntity, mUI.chkTickEntity);
    SetFlag(game::EntityPlacement::Flags::WantsKeyEvents, mUI.chkKeyEvents);
    SetFlag(game::EntityPlacement::Flags::WantsMouseEvents, mUI.chkMouseEvents);

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
    SetValue(mUI.entityLifetime, 0.0);
}

void DlgEntity::on_btnResetTag_clicked()
{
    mNodeClass.ResetTag();
    SetValue(mUI.entityTag, QString(""));
    SetPlaceholderText(mUI.entityTag, "Class Default");
}

void DlgEntity::on_btnEditVar_clicked()
{
    auto items = mUI.tableView->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;
    const auto index = items[0];
    const auto& var = mEntityClass.GetScriptVar(index.row());
    if (var.IsReadOnly())
        return;
    game::EntityPlacement::ScriptVarValue value;
    value.id    = var.GetId();
    value.value = var.GetVariantValue();
    if (const auto* val = mNodeClass.FindScriptVarValueById(value.id))
        value.value = val->value;

    std::vector<ResourceListItem> entities;
    std::vector<ResourceListItem> nodes;
    for (size_t i = 0; i < mEntityClass.GetNumNodes(); ++i)
    {
        const auto& node = mEntityClass.GetNode(i);
        ResourceListItem item;
        item.name = node.GetName();
        item.id   = node.GetId();
        nodes.push_back(std::move(item));
    }

    DlgScriptVal dlg(nodes, entities, mWorkspace->ListAllMaterials(),
                     this, value.value, var.IsArray());
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVars->SetValue(index.row(), value);

}
void DlgEntity::on_btnResetVar_clicked()
{
    auto items = mUI.tableView->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;
    const auto index = items[0];
    mScriptVars->DeleteValue(index.row());
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

void DlgEntity::on_entityTag_textChanged(const QString& text)
{
    mNodeClass.SetTag(GetValue(mUI.entityTag));
    SetPlaceholderText(mUI.entityTag, QString(""));
}

void DlgEntity::ScriptVariableSelectionChanged(const QItemSelection&, const QItemSelection&)
{
    SetEnabled(mUI.btnEditVar, false);
    SetEnabled(mUI.btnResetVar, false);

    auto items = mUI.tableView->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;
    const auto& var = mEntityClass.GetScriptVar(items[0].row());
    if (var.IsReadOnly())
        return;

    SetEnabled(mUI.btnEditVar, true);
    SetEnabled(mUI.btnResetVar, true);
}

void DlgEntity::SetFlag(game::EntityPlacement::Flags flag, QCheckBox* chk)
{
    if (chk->checkState() == Qt::PartiallyChecked)
        mNodeClass.ClearFlagSetting(flag);
    else mNodeClass.SetFlag(flag, GetValue(chk));
}

void DlgEntity::GetFlag(game::EntityPlacement::Flags flag, QCheckBox* chk)
{
    SetValue(chk, Qt::PartiallyChecked);
    if (mNodeClass.HasFlagSetting(flag))
        SetValue(chk, mNodeClass.TestFlag(flag));
}

} // namespace
