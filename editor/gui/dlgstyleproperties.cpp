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
#  include <QFileDialog>
#  include <QFileInfo>
#  include <Qt-Color-Widgets/include/ColorDialog>
#include "warnpop.h"

#include "base/utility.h"
#include "uikit/widget.h"
#include "engine/ui.h"
#include "engine/color.h"
#include "editor/app/utility.h"
#include "editor/gui/dlgstyleproperties.h"
#include "editor/gui/dlggradient.h"
#include "editor/gui/dlgfont.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"
#include "editor/app/workspace.h"
#include "editor/app/resource.h"

namespace gui
{
enum class PropertyType {
    Material, // null, color, gradient or material ref
    FontString,
    FontSize,
    VertTextAlign,
    HortTextAlign,
    Float,
    Bool,
    Color,
    Shape
};
enum class PropertySelector {
    Normal, Disabled, Focused, Moused, Pressed
};
std::string GetSelectorString(PropertySelector selector)
{
    if (selector == PropertySelector::Disabled)
        return "/disabled";
    else if (selector == PropertySelector::Focused)
        return "/focused";
    else if (selector == PropertySelector::Moused)
        return "/mouse-over";
    else if (selector == PropertySelector::Pressed)
        return "/pressed";
    return "";
}

struct Property {
    std::string key;
    PropertyType type = PropertyType::Material;
};
class DlgWidgetStyleProperties::PropertyModel : public QAbstractTableModel {
public:
    PropertyModel(const std::string& identifier,
                  std::vector<Property>&& props, engine::UIStyle* style, app::Workspace* workspace)
      : mWorkspace(workspace)
      , mIdentifier(identifier)
      , mProperties(std::move(props))
      , mStyle(style)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DisplayRole)
        {
            const auto col = index.column();
            const auto row = index.row();
            const auto& prop = mProperties[row];
            if (col == 0) return app::FromUtf8(prop.key);
            else if (col == 1) return app::toString(prop.type);
            else if (col == 2) return PropString(row, "");
            else if (col == 3) return PropString(row, "/disabled");
            else if (col == 4) return PropString(row, "/focused");
            else if (col == 5) return PropString(row, "/mouse-over");
            else if (col == 6) return PropString(row, "/pressed");
            else BUG("Unknown property table column index.");
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            if (section == 0) return "Key";
            else if (section == 1) return "Type";
            else if (section == 2) return "Normal";
            else if (section == 3) return "Disabled";
            else if (section == 4) return "Focused";
            else if (section == 5) return "Moused";
            else if (section == 6) return "Pressed";
            else BUG("Unknown property table column index.");
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mProperties.size());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 7;
    }
    const Property& GetProperty(size_t index) const
    {
        return base::SafeIndex(mProperties, index);
    }
    void UpdateRow(size_t row)
    {
        emit dataChanged(index(row, 0), index(row, 7));
    }
    void SetIdentifier(const std::string& identifier)
    {
        mIdentifier = identifier;

        emit dataChanged(index(0, 0), index(static_cast<int>(mProperties.size()), 7));
    }
private:
    QString PropString(int row, const char* selector) const
    {
        const auto& spec = mProperties[row];
        const auto type  = spec.type;
        const auto& property_key = mIdentifier + selector + "/" + spec.key;

        if (type == PropertyType::Material)
        {
            if (const auto* material = mStyle->GetMaterialType(property_key))
            {
                const auto type = material->GetType();
                if (type == engine::UIMaterial::Type::Null)
                    return "UI_None";
                else if (type == engine::UIMaterial::Type::Color)
                    return "UI_Color";
                else if (type == engine::UIMaterial::Type::Gradient)
                    return "UI_Gradient";
                else if (const auto* p = dynamic_cast<const engine::detail::UIMaterialReference*>(material))
                {
                    const auto& id = p->GetMaterialId();
                    const auto* resource = mWorkspace->FindResourceById(app::FromUtf8(id));
                    if (resource)
                        return resource->GetName();

                    return "Broken material ref!!";
                }
            }
        }
        else if (const auto& prop = mStyle->GetProperty(property_key))
        {
            if (type == PropertyType::FontString)
                return app::toString(prop.GetValue<std::string>());
            else if (type == PropertyType::FontSize)
                return app::toString(prop.GetValue<int>());
            else if (type == PropertyType::VertTextAlign)
                return app::toString(prop.GetValue<engine::UIStyle::VerticalTextAlign>());
            else if (type == PropertyType::HortTextAlign)
                return app::toString(prop.GetValue<engine::UIStyle::HorizontalTextAlign>());
            else if (type == PropertyType::Color)
                return app::toString(prop.GetValue<engine::Color4f>());
            else if (type == PropertyType::Bool)
                return app::toString(prop.GetValue<bool>());
            else if (type == PropertyType::Float)
                return app::toString(prop.GetValue<float>());
            else if (type == PropertyType::Shape)
                return app::toString(prop.GetValue<engine::UIStyle::WidgetShape>());
            else BUG("Unhandled property type.");
        }
        return QString();
    }

private:
    const app::Workspace* mWorkspace = nullptr;
    std::string mIdentifier;
    std::vector<Property> mProperties;
    engine::UIStyle* mStyle = nullptr;
    uik::Widget* mWidget = nullptr;
};

DlgWidgetStyleProperties::DlgWidgetStyleProperties(QWidget* parent, engine::UIStyle* style,  app::Workspace* workspace, std::string identifier)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mStyle(style)
  , mIdentifier(std::move(identifier))
{
    mUI.setupUi(this);
    PopulateFromEnum<PropertySelector>(mUI.cmbSelector);
    PopulateFromEnum<engine::UIStyle::VerticalTextAlign>(mUI.widgetTextVAlign);
    PopulateFromEnum<engine::UIStyle::HorizontalTextAlign>(mUI.widgetTextHAlign);
    PopulateFromEnum<engine::UIStyle::WidgetShape>(mUI.widgetShape);
    PopulateFontNames(mUI.widgetFontName);
    PopulateFontSizes(mUI.widgetFontSize);
    SetVisible(mUI.lblWidgetClass, false);
    SetVisible(mUI.cmbWidgetClass, false);
    SetVisible(mUI.widgetFontName, false);
    SetVisible(mUI.btnOpenFontFile, false);
    SetVisible(mUI.btnSelectFont, false);
    SetVisible(mUI.btnSelectMaterial, false);
    SetVisible(mUI.widgetFontSize, false);
    SetVisible(mUI.widgetTextVAlign, false);
    SetVisible(mUI.widgetTextHAlign, false);
    SetVisible(mUI.widgetColor, false);
    SetVisible(mUI.widgetFlag, false);
    SetVisible(mUI.widgetFloat, false);
    SetVisible(mUI.widgetShape, false);
    SetVisible(mUI.widgetMaterial, false);
    SetEnabled(mUI.btnResetProperty, false);
    SetEnabled(mUI.cmbSelector, false);

    if (mIdentifier.empty())
    {
        SetVisible(mUI.lblWidgetClass, true);
        SetVisible(mUI.cmbWidgetClass, true);
        mIdentifier = "window/checkbox";
    }

    QMenu* menu = new QMenu(this);
    QAction* set_material = menu->addAction(QIcon("icons:material.png"), "Material");
    QAction* set_color    = menu->addAction(QIcon("icons:color_wheel.png"), "Color");
    QAction* set_gradient = menu->addAction(QIcon("icons:color_gradient.png"), "Gradient");
    connect(set_material, &QAction::triggered, this, &DlgWidgetStyleProperties::SetWidgetMaterial);
    connect(set_color,    &QAction::triggered, this, &DlgWidgetStyleProperties::SetWidgetColor);
    connect(set_gradient, &QAction::triggered, this, &DlgWidgetStyleProperties::SetWidgetGradient);
    mUI.btnSelectMaterial->setMenu(menu);

    std::vector<Property> props = {
        {"background",                    PropertyType::Material},
        {"shape",                         PropertyType::Shape},
        {"border",                        PropertyType::Material},
        {"border-width",                  PropertyType::Float},
        {"text-color",                    PropertyType::Color},
        {"text-blink",                    PropertyType::Bool},
        {"text-underline",                PropertyType::Bool},
        {"text-font",                     PropertyType::FontString},
        {"text-size",                     PropertyType::FontSize},
        {"text-vertical-align",           PropertyType::VertTextAlign},
        {"text-horizontal-align",         PropertyType::HortTextAlign},
        {"edit-text-color",               PropertyType::Color},
        {"edit-text-font",                PropertyType::FontString},
        {"edit-text-size",                PropertyType::FontSize},
        {"text-edit-background",          PropertyType::Material},
        {"text-edit-shape",               PropertyType::Shape},
        {"text-edit-border",              PropertyType::Material},
        {"text-edit-border-width",        PropertyType::Float},
        {"check-background",              PropertyType::Material},
        {"check-background-shape",        PropertyType::Shape},
        {"check-border",                  PropertyType::Material},
        {"check-border-width",            PropertyType::Float},
        {"check-mark",                    PropertyType::Material},
        {"check-shape",                   PropertyType::Shape},
        {"button-background",             PropertyType::Material},
        {"button-shape",                  PropertyType::Shape},
        {"button-border",                 PropertyType::Material},
        {"button-border-width",           PropertyType::Float},
        {"button-icon",                   PropertyType::Material},
        {"slider-background",             PropertyType::Material},
        {"slider-background-shape",       PropertyType::Shape},
        {"slider-knob",                   PropertyType::Material},
        {"slider-knob-shape",             PropertyType::Shape},
        {"slider-knob-border",            PropertyType::Material},
        {"slider-knob-border-width",      PropertyType::Float},
        {"slider-border",                 PropertyType::Material},
        {"slider-border-width",           PropertyType::Float},
        {"progress-bar-background",       PropertyType::Material},
        {"progress-bar-background-shape", PropertyType::Shape},
        {"progress-bar-fill",             PropertyType::Material},
        {"progress-bar-fill-shape",       PropertyType::Shape},
        {"progress-bar-border",           PropertyType::Material},
        {"progress-bar-border-width",     PropertyType::Float}
    };
    std::sort(props.begin(), props.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.key < rhs.key;
    });

    mModel.reset(new PropertyModel(mIdentifier, std::move(props), style, workspace));
    mUI.tableView->setModel(mModel.get());
    mUI.tableView->setColumnWidth(0, 300);
    connect(mUI.tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &DlgWidgetStyleProperties::TableSelectionChanged);

    QByteArray geometry;
    if (mWorkspace->GetUserProperty("dlg_widget_style_property_geometry", &geometry))
        restoreGeometry(geometry);
}

DlgWidgetStyleProperties::~DlgWidgetStyleProperties() = default;

void DlgWidgetStyleProperties::SetMaterials(const std::vector<app::ListItem>& list)
{
    SetList(mUI.widgetMaterial, list);
}

void DlgWidgetStyleProperties::ShowPropertyValue()
{
    SetVisible(mUI.widgetFontName, false);
    SetVisible(mUI.btnOpenFontFile, false);
    SetVisible(mUI.btnSelectFont, false);
    SetVisible(mUI.btnSelectMaterial, false);
    SetVisible(mUI.widgetMaterial, false);
    SetVisible(mUI.widgetFontSize, false);
    SetVisible(mUI.widgetTextVAlign, false);
    SetVisible(mUI.widgetTextHAlign, false);
    SetVisible(mUI.widgetColor, false);
    SetVisible(mUI.widgetFloat, false);
    SetVisible(mUI.widgetFlag, false);
    SetVisible(mUI.widgetShape, false);
    SetEnabled(mUI.btnResetProperty, false);
    SetEnabled(mUI.cmbSelector, false);

    SetValue(mUI.widgetFontName, -1);
    SetValue(mUI.widgetFontSize, -1);
    SetValue(mUI.widgetTextHAlign, -1);
    SetValue(mUI.widgetTextVAlign, -1);
    SetValue(mUI.widgetColor, engine::Color::White);
    SetValue(mUI.widgetFlag, Qt::PartiallyChecked);
    SetValue(mUI.widgetFloat, 0.0f);
    SetValue(mUI.widgetShape, -1);
    SetValue(mUI.widgetMaterial, -1);

    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    const auto& prop = mModel->GetProperty(indices[0].row());
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key = mIdentifier + selector + "/" + prop.key;
    const auto type = prop.type;

    if (type == PropertyType::Material)
    {
        SetVisible(mUI.widgetMaterial, true);
        SetVisible(mUI.btnSelectMaterial, true);
        if (const auto* material = mStyle->GetMaterialType(property_key))
        {
            const auto type = material->GetType();
            if (type == engine::UIMaterial::Type::Null)
                SetValue(mUI.widgetMaterial, QString("UI_None"));
            else if (type == engine::UIMaterial::Type::Color)
                SetValue(mUI.widgetMaterial, QString("UI_Color"));
            else if (type == engine::UIMaterial::Type::Gradient)
                SetValue(mUI.widgetMaterial, QString("UI_Gradient"));
            else if (const auto* p = dynamic_cast<const engine::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetMaterial, ListItemId(p->GetMaterialId()));
        }
    }
    else if (type == PropertyType::FontString)
    {
        SetVisible(mUI.widgetFontName, true);
        SetVisible(mUI.btnOpenFontFile, true);
        SetVisible(mUI.btnSelectFont, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetFontName, prop.GetValue<std::string>());
    }
    else if (type == PropertyType::FontSize)
    {
        SetVisible(mUI.widgetFontSize, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetFontSize, prop.GetValue<int>());
    }
    else if (type == PropertyType::VertTextAlign)
    {
        SetVisible(mUI.widgetTextVAlign, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetTextVAlign, prop.GetValue<engine::UIStyle::VerticalTextAlign>());
    }
    else if (type == PropertyType::HortTextAlign)
    {
        SetVisible(mUI.widgetTextHAlign, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetTextHAlign, prop.GetValue<engine::UIStyle::HorizontalTextAlign>());
    }
    else if (type == PropertyType::Color)
    {
        SetVisible(mUI.widgetColor, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetColor, prop.GetValue<engine::Color4f>());
    }
    else if (type == PropertyType::Bool)
    {
        SetVisible(mUI.widgetFlag, true);
        mUI.widgetFlag->setText(app::FromUtf8(prop.key));
        if (const auto& prop = mStyle->GetProperty(property_key))
        {
            const auto value = prop.GetValue<bool>();
            SetValue(mUI.widgetFlag, value ? Qt::Checked : Qt::Unchecked);
        }
    }
    else if (type == PropertyType::Float)
    {
        SetVisible(mUI.widgetFloat, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetFloat, prop.GetValue<float>());
    }
    else if (type == PropertyType::Shape)
    {
        SetVisible(mUI.widgetShape, true);
        if (const auto& prop = mStyle->GetProperty(property_key))
            SetValue(mUI.widgetShape, prop.GetValue<engine::UIStyle::WidgetShape>());
    }
    mUI.grpValue->setTitle(app::FromUtf8(prop.key));

    SetEnabled(mUI.btnResetProperty, true);
    SetEnabled(mUI.cmbSelector, true);
}

void DlgWidgetStyleProperties::SetPropertyValue()
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    const auto row = indices[0].row();
    const auto& prop = mModel->GetProperty(row);
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key = mIdentifier + selector + "/" + prop.key;
    const auto type  = prop.type;

    if (type == PropertyType::Material)
    {
        if (mUI.widgetMaterial->currentIndex() == 1)
            mStyle->DeleteMaterial(property_key);
        else if (mUI.widgetMaterial->currentIndex() == 0)
            mStyle->SetMaterial(property_key, engine::detail::UINullMaterial());
        else if (mUI.widgetMaterial->currentIndex() == 1)
            mStyle->SetMaterial(property_key, engine::detail::UIColor());
        else if (mUI.widgetMaterial->currentIndex() == 2)
            mStyle->SetMaterial(property_key, engine::detail::UIGradient());
        else mStyle->SetMaterial(property_key, engine::detail::UIMaterialReference(GetItemId(mUI.widgetMaterial)));

        mPainter->DeleteMaterialInstanceByKey(property_key);
    }
    else if (type == PropertyType::FontString)
    {
        mStyle->SetProperty(property_key, GetValue(mUI.widgetFontName));
    }
    else if (type == PropertyType::FontSize)
    {
        mStyle->SetProperty(property_key, (int)GetValue(mUI.widgetFontSize));
    }
    else if (type == PropertyType::VertTextAlign)
    {
        mStyle->SetProperty(property_key, (engine::UIStyle::VerticalTextAlign)GetValue(mUI.widgetTextVAlign));
    }
    else if (type == PropertyType::HortTextAlign)
    {
        mStyle->SetProperty(property_key, (engine::UIStyle::HorizontalTextAlign)GetValue(mUI.widgetTextHAlign));
    }
    else if (type == PropertyType::Color)
    {
        mStyle->SetProperty(property_key, (engine::Color4f)GetValue(mUI.widgetColor));
    }
    else if (type == PropertyType::Bool)
    {
        const int check_state = mUI.widgetFlag->checkState();
        if (check_state == Qt::PartiallyChecked)
            mStyle->DeleteProperty(property_key);
        else if (check_state == Qt::Checked)
            mStyle->SetProperty(property_key, true);
        else if (check_state == Qt::Unchecked)
            mStyle->SetProperty(property_key, false);
    }
    else if (type == PropertyType::Float)
    {
        mStyle->SetProperty(property_key, (float)GetValue(mUI.widgetFloat));
    }
    else if (type == PropertyType::Shape)
    {
        mStyle->SetProperty(property_key, (engine::UIStyle::WidgetShape)GetValue(mUI.widgetShape));
    }
    mModel->UpdateRow(row);
}

void DlgWidgetStyleProperties::on_btnAccept_clicked()
{
    mWorkspace->SetUserProperty("dlg_widget_style_property_geometry", saveGeometry());

    accept();
}
void DlgWidgetStyleProperties::on_btnCancel_clicked()
{
    mWorkspace->SetUserProperty("dlg_widget_style_property_geometry", saveGeometry());

    reject();
}

void DlgWidgetStyleProperties::on_btnOpenFontFile_clicked()
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Font File"), "", tr("Font (*.ttf *.otf)"));
    if (list.isEmpty())
        return;
    const auto& file = mWorkspace->MapFileToWorkspace(list[0]);
    SetValue(mUI.widgetFontName, file);
    SetPropertyValue();
}
void DlgWidgetStyleProperties::on_btnSelectFont_clicked()
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    const auto row = indices[0].row();
    const auto& prop = mModel->GetProperty(row);
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key  = mIdentifier + selector + "/" + prop.key;

    QString font = GetValue(mUI.widgetFontName);
    if (font.isEmpty())
    {
        if (const auto& prop = mStyle->GetProperty(property_key))
            font = app::FromUtf8(prop.GetValue<std::string>());
    }
    DlgFont::DisplaySettings disp;
    disp.font_size  = 18;
    disp.underline  = false;
    disp.blinking   = false;
    disp.text_color = Qt::darkGray;
    DlgFont dlg(this, mWorkspace, font, disp);
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.widgetFontName, dlg.GetSelectedFontURI());
    SetPropertyValue();
}

void DlgWidgetStyleProperties::on_btnResetProperty_clicked()
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    const auto row = indices[0].row();
    const auto& prop = mModel->GetProperty(row);
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key  = mIdentifier + selector + "/" + prop.key;

    if (prop.type == PropertyType::Material)
    {
        mStyle->DeleteMaterial(property_key);
        // purge old material instances if any.
        mPainter->DeleteMaterialInstanceByKey(property_key);
    }
    else
    {
        mStyle->DeleteProperty(property_key);
    }
    mModel->UpdateRow(row);

    ShowPropertyValue();
}

void DlgWidgetStyleProperties::on_cmbWidgetClass_currentIndexChanged(int)
{
    const std::string& klass = GetValue(mUI.cmbWidgetClass);
    mIdentifier = "window/" + klass,
    mModel->SetIdentifier(mIdentifier);
}

void DlgWidgetStyleProperties::on_cmbSelector_currentIndexChanged(int)
{
    ShowPropertyValue();
}

void DlgWidgetStyleProperties::on_widgetFontName_currentIndexChanged(int)
{
    SetPropertyValue();
}
void DlgWidgetStyleProperties::on_widgetFontSize_currentIndexChanged(int)
{
    SetPropertyValue();
}
void DlgWidgetStyleProperties::on_widgetTextVAlign_currentIndexChanged(int)
{
    SetPropertyValue();
}
void DlgWidgetStyleProperties::on_widgetTextHAlign_currentIndexChanged(int)
{
    SetPropertyValue();
}

void DlgWidgetStyleProperties::on_widgetColor_colorChanged(QColor color)
{
    SetPropertyValue();
}

void DlgWidgetStyleProperties::on_widgetFlag_stateChanged(int)
{
    SetPropertyValue();
}

void DlgWidgetStyleProperties::on_widgetFloat_valueChanged(double)
{
    SetPropertyValue();
}
void DlgWidgetStyleProperties::on_widgetShape_currentIndexChanged(int)
{
    SetPropertyValue();
}
void DlgWidgetStyleProperties::on_widgetMaterial_currentIndexChanged(int)
{
    SetPropertyValue();
}

void DlgWidgetStyleProperties::TableSelectionChanged(const QItemSelection, const QItemSelection)
{
    ShowPropertyValue();
}

void DlgWidgetStyleProperties::SetWidgetMaterial()
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    DlgMaterial dlg(this, mWorkspace, GetItemId(mUI.widgetMaterial));
    if (dlg.exec() == QDialog::Rejected)
        return;

    const auto row = indices[0].row();
    const auto& prop = mModel->GetProperty(row);
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key = mIdentifier + selector + "/" + prop.key;

    SetValue(mUI.widgetMaterial, ListItemId(dlg.GetSelectedMaterialId()));

    mStyle->SetMaterial(property_key, engine::detail::UIMaterialReference(GetItemId(mUI.widgetMaterial)));
    mPainter->DeleteMaterialInstanceByKey(property_key);
    mModel->UpdateRow(row);

    ShowPropertyValue();
}
void DlgWidgetStyleProperties::SetWidgetColor()
{
    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    const auto row   = indices[0].row();
    const auto& prop = mModel->GetProperty(row);
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key = mIdentifier + selector + "/" + prop.key;

    color_widgets::ColorDialog dlg(this);
    dlg.setAlphaEnabled(false);
    dlg.setButtonMode(color_widgets::ColorDialog::ButtonMode::OkCancel);
    if (const auto* material = mStyle->GetMaterialType(property_key))
    {
        if (const auto* p = dynamic_cast<const engine::detail::UIColor*>(material))
            dlg.setColor(FromGfx(p->GetColor()));
    }
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.widgetMaterial, QString("UI_Color"));

    mStyle->SetMaterial(property_key, engine::detail::UIColor(ToGfx(dlg.color())));
    mPainter->DeleteMaterialInstanceByKey(property_key);
    mModel->UpdateRow(row);

    ShowPropertyValue();
}
void DlgWidgetStyleProperties::SetWidgetGradient()
{
    using Index = engine::detail::UIGradient::ColorIndex;

    const auto& indices = mUI.tableView->selectionModel()->selectedRows();
    if (indices.empty())
        return;

    const auto row = indices[0].row();
    const auto& prop = mModel->GetProperty(row);
    const auto& selector = GetSelectorString((PropertySelector)GetValue(mUI.cmbSelector));
    const auto& property_key = mIdentifier + selector + "/" + prop.key;

    DlgGradient dlg(this);
    if (const auto* material = mStyle->GetMaterialType(property_key))
    {
        if (const auto* p = dynamic_cast<const engine::detail::UIGradient*>(material))
        {
            const auto color_top_left = p->GetColor(Index::TopLeft);
            const auto color_top_right = p->GetColor(Index::TopRight);
            const auto color_bot_left = p->GetColor(Index::BottomLeft);
            const auto color_bot_right = p->GetColor(Index::BottomRight);

            dlg.SetColor(FromGfx(color_top_left), 0);
            dlg.SetColor(FromGfx(color_top_right), 1);
            dlg.SetColor(FromGfx(color_bot_left), 2);
            dlg.SetColor(FromGfx(color_bot_right), 3);
        }
    }
    if (dlg.exec() == QDialog::Rejected)
        return;

    engine::detail::UIGradient gradient;
    gradient.SetColor(ToGfx(dlg.GetColor(0)), Index::TopLeft);
    gradient.SetColor(ToGfx(dlg.GetColor(1)), Index::TopRight);
    gradient.SetColor(ToGfx(dlg.GetColor(2)), Index::BottomLeft);
    gradient.SetColor(ToGfx(dlg.GetColor(3)), Index::BottomRight);
    mStyle->SetMaterial(property_key, std::move(gradient));

    mPainter->DeleteMaterialInstanceByKey(property_key);

    mModel->UpdateRow(row);

    ShowPropertyValue();
}

} // namespace
