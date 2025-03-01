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
#  include <QStringList>
#  include <boost/algorithm/string/erase.hpp>
#  include <Qt-Color-Widgets/include/ColorDialog>
#include "warnpop.h"

#include <optional>

#include "uikit/widget.h"
#include "engine/ui.h"
#include "engine/color.h"
#include "editor/app/resource-uri.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/widgetstylewidget.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/dlgfont.h"
#include "editor/gui/dlggradient.h"
#include "editor/gui/dlgimgview.h"
#include "editor/gui/utility.h"

namespace gui
{
WidgetStyleWidget::WidgetStyleWidget(QWidget* parent) : QWidget(parent)
{
    mUI.setupUi(this);
    PopulateFromEnum<engine::UIStyle::VerticalTextAlign>(mUI.widgetTextVAlign);
    PopulateFromEnum<engine::UIStyle::HorizontalTextAlign>(mUI.widgetTextHAlign);

    {
        QMenu* menu = new QMenu(this);
        QAction* set_background_material = menu->addAction(QIcon("icons:material.png"), "Material");
        QAction* set_background_color    = menu->addAction(QIcon("icons:color_wheel.png"), "Color");
        QAction* set_background_gradient = menu->addAction(QIcon("icons:color_gradient.png"), "Gradient");
        QAction* set_background_image    = menu->addAction(QIcon("icons:image.png"), "Image");
        connect(set_background_material, &QAction::triggered, this, &WidgetStyleWidget::SetBackgroundMaterial);
        connect(set_background_color,    &QAction::triggered, this, &WidgetStyleWidget::SetBackgroundColor);
        connect(set_background_gradient, &QAction::triggered, this, &WidgetStyleWidget::SetBackgroundGradient);
        connect(set_background_image,    &QAction::triggered, this, &WidgetStyleWidget::SetBackgroundImage);
        mUI.btnSelectWidgetBackground->setMenu(menu);
    }

    {
        QMenu* menu = new QMenu(this);
        QAction* set_border_material = menu->addAction(QIcon("icons:material.png"), "Material");
        QAction* set_border_color    = menu->addAction(QIcon("icons:color_wheel.png"), "Color");
        QAction* set_border_gradient = menu->addAction(QIcon("icons:color_gradient.png"), "Gradient");
        QAction* set_border_image    = menu->addAction(QIcon("icons:image.png"), "Image");
        connect(set_border_material, &QAction::triggered, this, &WidgetStyleWidget::SetBorderMaterial);
        connect(set_border_color,    &QAction::triggered, this, &WidgetStyleWidget::SetBorderColor);
        connect(set_border_gradient, &QAction::triggered, this, &WidgetStyleWidget::SetBorderGradient);
        connect(set_border_image,    &QAction::triggered, this, &WidgetStyleWidget::SetBorderImage);
        mUI.btnSelectWidgetBorder->setMenu(menu);
    }

    PopulateFontNames(mUI.widgetFontName);
    PopulateFontSizes(mUI.widgetFontSize);

    mUI.widgetFontName->lineEdit()->setReadOnly(true);
}

void WidgetStyleWidget::RebuildMaterialCombos(const std::vector<ResourceListItem>& list)
{
    SetList(mUI.widgetBackground, list);
    SetList(mUI.widgetBorder, list);
}

bool WidgetStyleWidget::IsUnderEdit() const
{
    // check if the color dialog is open for continuous edit (the color change signal
    // is emitted while the dialog is open). Ultimately this will block undo stack
    // copying since the undo stack should take snapshots at discrete change steps.
    return mUI.widgetTextColor->isDialogOpen();
}

void WidgetStyleWidget::on_widgetFontName_currentIndexChanged(int)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetFontSize_currentIndexChanged(int)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextVAlign_currentIndexChanged(int)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextHAlign_currentIndexChanged(int)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextBlink_stateChanged(int)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextUnderline_stateChanged(int)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextColor_colorChanged(QColor color)
{
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_widgetBackground_currentIndexChanged(int)
{
    if (mUI.widgetBackground->currentIndex() == -1)
        DeleteMaterial(MapProperty("/background"));
    else if (mUI.widgetBackground->currentIndex() == 0)
        SetMaterial(MapProperty("/background"), engine::detail::UINullMaterial());
    else if (mUI.widgetBackground->currentIndex() == 1)
        SetMaterial(MapProperty("/background"), engine::detail::UIColor());
    else if (mUI.widgetBackground->currentIndex() == 2)
        SetMaterial(MapProperty("/background"), engine::detail::UIGradient());
    else if (mUI.widgetBackground->currentIndex() == 3)
        SetMaterial(MapProperty("/background"), engine::detail::UITexture(res::Checkerboard));
    else SetMaterial(MapProperty("/background"), engine::detail::UIMaterialReference(GetItemId(mUI.widgetBackground)));

    UpdateWidgetStyleString();
}
void WidgetStyleWidget::on_widgetBorder_currentIndexChanged(int)
{
    if (mUI.widgetBorder->currentIndex() == -1)
        DeleteMaterial(MapProperty("/border"));
    else if (mUI.widgetBorder->currentIndex() == 0)
        SetMaterial(MapProperty("/border"), engine::detail::UINullMaterial());
    else if (mUI.widgetBorder->currentIndex() == 1)
        SetMaterial(MapProperty("/border"), engine::detail::UIColor());
    else if (mUI.widgetBorder->currentIndex() == 2)
        SetMaterial(MapProperty("/border"), engine::detail::UIGradient());
    else if (mUI.widgetBorder->currentIndex() == 3)
        SetMaterial(MapProperty("/border"), engine::detail::UITexture(res::Checkerboard));
    else SetMaterial(MapProperty("/border"), engine::detail::UIMaterialReference(GetItemId(mUI.widgetBorder)));

    UpdateWidgetStyleString();
}
void WidgetStyleWidget::on_btnResetWidgetFontName_clicked()
{
    SetValue(mUI.widgetFontName, -1);
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextColor_clicked()
{
    mUI.widgetTextColor->clearColor();
    UpdateWidgetProperties();
}

void WidgetStyleWidget::on_btnSelectAppFont_clicked()
{
    if (!mWidget)
        return;

    QString font = GetValue(mUI.widgetFontName);
    if (font.isEmpty())
    {
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-font")))
            font = app::FromUtf8(prop.GetValue<std::string>());
    }
    DlgFont::DisplaySettings disp;
    disp.font_size = 18;
    disp.underline = false;
    disp.blinking  = false;
    disp.text_color = Qt::darkGray;
    if (mUI.widgetFontSize->currentIndex() == -1) {
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-size")))
            disp.font_size = prop.GetValue<int>();
    } else disp.font_size = GetValue(mUI.widgetFontSize);

    const auto underline_state = mUI.widgetTextUnderline->checkState();
    if (underline_state == Qt::PartiallyChecked) {
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-underline")))
            disp.underline = prop.GetValue<bool>();
    } else if (underline_state == Qt::Checked)
        disp.underline = true;
    else disp.underline = false;

    if (!mUI.widgetTextColor->hasColor()) {
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-color")))
            disp.text_color = FromGfx(prop.GetValue<engine::Color4f>());
    } else  disp.text_color = GetValue(mUI.widgetTextColor);

    DlgFont dlg(mParent, mWorkspace, font, disp);
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.widgetFontName, dlg.GetSelectedFontURI());
    UpdateWidgetProperties();
}

void WidgetStyleWidget::on_btnSelectCustomFont_clicked()
{
    if (!mWidget)
        return;

    const auto& list = QFileDialog::getOpenFileName(this,
        tr("Select Font File"), "", tr("Font (*.ttf *.otf *.json)"));
    if (list.isEmpty())
        return;
    const auto& file = mWorkspace->MapFileToWorkspace(list);
    SetValue(mUI.widgetFontName, file);
    UpdateWidgetProperties();
}

void WidgetStyleWidget::on_btnResetWidgetFontSize_clicked()
{
    SetValue(mUI.widgetFontSize, -1);
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextVAlign_clicked()
{
    SetValue(mUI.widgetTextVAlign, -1);
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextHAlign_clicked()
{
    SetValue(mUI.widgetTextHAlign, -1);
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextProp_clicked()
{
    SetValue(mUI.widgetTextUnderline, Qt::PartiallyChecked);
    SetValue(mUI.widgetTextBlink, Qt::PartiallyChecked);
    UpdateWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetBackground_clicked()
{
    SetValue(mUI.widgetBackground, -1);
    on_widgetBackground_currentIndexChanged(-1);
}
void WidgetStyleWidget::on_btnResetWidgetBorder_clicked()
{
    SetValue(mUI.widgetBorder, -1);
    on_widgetBorder_currentIndexChanged(-1);
}

void WidgetStyleWidget::SetBackgroundMaterial()
{
    if (mWidget)
    {
        DlgMaterial dlg(mParent, mWorkspace, GetItemId(mUI.widgetBackground));
        if (dlg.exec() == QDialog::Rejected)
            return;
        SetValue(mUI.widgetBackground, ListItemId(dlg.GetSelectedMaterialId()));

        mStyle->SetMaterial(MapProperty("/background"),
            engine::detail::UIMaterialReference(GetItemId(mUI.widgetBackground)));

        mPainter->DeleteMaterialInstanceByKey(MapProperty("/background"));

        UpdateWidgetStyleString();

        ShowWidgetProperties();

        emit StyleEdited();
    }
}
void WidgetStyleWidget::SetBackgroundColor()
{
    SetMaterialColor("/background");
}
void WidgetStyleWidget::SetBackgroundGradient()
{
    SetMaterialGradient("/background");
}

void WidgetStyleWidget::SetBackgroundImage()
{
    SetMaterialImage("/background");
}

void WidgetStyleWidget::SetBorderMaterial()
{
    if (mWidget)
    {
        DlgMaterial dlg(mParent, mWorkspace, GetItemId(mUI.widgetBorder));
        if (dlg.exec() == QDialog::Rejected)
            return;
        SetValue(mUI.widgetBorder, ListItemId(dlg.GetSelectedMaterialId()));

        mStyle->SetMaterial(MapProperty("/border"),
            engine::detail::UIMaterialReference(GetItemId(mUI.widgetBorder)));

        mPainter->DeleteMaterialInstanceByKey(MapProperty("/border"));

        UpdateWidgetStyleString();

        ShowWidgetProperties();

        emit StyleEdited();
    }
}
void WidgetStyleWidget::SetBorderColor()
{
    SetMaterialColor("/border");
}
void WidgetStyleWidget::SetBorderGradient()
{
    SetMaterialGradient("/border");
}

void WidgetStyleWidget::SetBorderImage()
{
    SetMaterialImage("/border");
}

void WidgetStyleWidget::ShowWidgetProperties()
{
    SetValue(mUI.widgetFontName, -1);
    SetValue(mUI.widgetFontSize, -1);
    SetValue(mUI.widgetTextVAlign, -1);
    SetValue(mUI.widgetTextHAlign, -1);
    SetValue(mUI.widgetTextColor, engine::Color::White);
    SetValue(mUI.widgetTextBlink, Qt::PartiallyChecked);
    SetValue(mUI.widgetTextUnderline, Qt::PartiallyChecked);
    SetValue(mUI.widgetBackground, -1);
    SetValue(mUI.widgetBorder, -1);
    mUI.widgetTextColor->clearColor();

    if (mWidget)
    {
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-font")))
            SetValue(mUI.widgetFontName, prop.GetValue<std::string>());
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-size")))
            SetValue(mUI.widgetFontSize, QString::number(prop.GetValue<int>()));

        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-vertical-align")))
            SetValue(mUI.widgetTextVAlign , prop.GetValue<engine::UIStyle::VerticalTextAlign>());
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-horizontal-align")))
            SetValue(mUI.widgetTextHAlign , prop.GetValue<engine::UIStyle::HorizontalTextAlign>());
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-color")))
            SetValue(mUI.widgetTextColor , prop.GetValue<engine::Color4f>());

        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-blink")))
        {
            const auto value = prop.GetValue<bool>();
            SetValue(mUI.widgetTextBlink , value ? Qt::Checked : Qt::Unchecked);
        }
        if (const auto& prop = mStyle->GetProperty(MapProperty("/text-underline")))
        {
            const auto value = prop.GetValue<bool>();
            SetValue(mUI.widgetTextUnderline , value ? Qt::Checked : Qt::Unchecked);
        }

        if (const auto* material = mStyle->GetMaterialType(MapProperty("/background")))
        {
            const auto type = material->GetType();
            if (type == engine::UIMaterial::Type::Null)
                SetValue(mUI.widgetBackground, ListItemId("_ui_none"));
            else if (type == engine::UIMaterial::Type::Color)
                SetValue(mUI.widgetBackground, ListItemId("_ui_color"));
            else if (type == engine::UIMaterial::Type::Gradient)
                SetValue(mUI.widgetBackground, ListItemId("_ui_gradient"));
            else if (type == engine::UIMaterial::Type::Texture)
                SetValue(mUI.widgetBackground, ListItemId("_ui_image"));
            else if (const auto* p = dynamic_cast<const engine::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetBackground, ListItemId(p->GetMaterialId()));
        }
        if (const auto* material = mStyle->GetMaterialType(MapProperty("/border")))
        {
            const auto type = material->GetType();
            if (type == engine::UIMaterial::Type::Null)
                SetValue(mUI.widgetBorder, ListItemId("_ui_none"));
            else if (type == engine::UIMaterial::Type::Color)
                SetValue(mUI.widgetBorder, ListItemId("_ui_color"));
            else if (type == engine::UIMaterial::Type::Gradient)
                SetValue(mUI.widgetBorder, ListItemId("_ui_gradient"));
            else if (type == engine::UIMaterial::Type::Texture)
                SetValue(mUI.widgetBorder, ListItemId("_ui_image"));
            else if (const auto* p = dynamic_cast<const engine::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetBorder, ListItemId(p->GetMaterialId()));
        }
    }
}

void WidgetStyleWidget::UpdateWidgetProperties()
{
    if (mWidget)
    {
        // set style properties
        const std::string& font = GetValue(mUI.widgetFontName); // msvs build workaround
        if (font.empty())
            mStyle->DeleteProperty(MapProperty("/text-font"));
        else mStyle->SetProperty(MapProperty("/text-font"), font);

        if (mUI.widgetFontSize->currentIndex() == -1)
            mStyle->DeleteProperty(MapProperty("/text-size"));
        else mStyle->SetProperty(MapProperty("/text-size"), (int)GetValue(mUI.widgetFontSize));

        if (mUI.widgetTextVAlign->currentIndex() == -1)
            mStyle->DeleteProperty(MapProperty("/text-vertical-align"));
        else mStyle->SetProperty(MapProperty("/text-vertical-align"),(engine::UIStyle::VerticalTextAlign)GetValue(mUI.widgetTextVAlign));

        if (mUI.widgetTextHAlign->currentIndex() == -1)
            mStyle->DeleteProperty(MapProperty("/text-horizontal-align"));
        else mStyle->SetProperty(MapProperty("/text-horizontal-align"), (engine::UIStyle::HorizontalTextAlign)GetValue(mUI.widgetTextHAlign));

        if (!mUI.widgetTextColor->hasColor())
            mStyle->DeleteProperty(MapProperty("/text-color"));
        else mStyle->SetProperty(MapProperty("/text-color"), (gfx::Color4f)GetValue(mUI.widgetTextColor));

        const auto blink_state = mUI.widgetTextBlink->checkState();
        if (blink_state == Qt::PartiallyChecked)
            mStyle->DeleteProperty(MapProperty("/text-blink"));
        else if (blink_state == Qt::Checked)
            mStyle->SetProperty(MapProperty("/text-blink"), true);
        else if (blink_state == Qt::Unchecked)
            mStyle->SetProperty(MapProperty("/text-blink"), false);

        const auto underline_state = mUI.widgetTextUnderline->checkState();
        if (underline_state == Qt::PartiallyChecked)
            mStyle->DeleteProperty(MapProperty("/text-underline"));
        else if (underline_state == Qt::Checked)
            mStyle->SetProperty(MapProperty("/text-underline"), true);
        else if (underline_state == Qt::Unchecked)
            mStyle->SetProperty(MapProperty("/text-underline"), false);

        UpdateWidgetStyleString();

        emit StyleEdited();
    }
}

void WidgetStyleWidget::UpdateWidgetStyleString()
{
    // update widget's style string so that it contains the
    // latest widget style.
    const auto& id = mWidget->GetId();

    // gather the style properties for this widget into a single style string
    // in the styling engine specific format.
    auto style = mStyle->MakeStyleString(id);
    // this is a bit of a hack but we know that the style string
    // contains the widget id for each property. removing the
    // widget id from the style properties:
    // a) saves some space
    // b) makes the style string copyable from one widget to another as-s
    boost::erase_all(style, id + "/");
    // set the actual style string.
    mWidget->SetStyleString(style);
}

void WidgetStyleWidget::SetMaterialColor(const char* key)
{
    if (mWidget)
    {
        color_widgets::ColorDialog dlg(this);
        dlg.setAlphaEnabled(true);
        dlg.setButtonMode(color_widgets::ColorDialog::ButtonMode::OkCancel);

        connect(&dlg, &color_widgets::ColorDialog::colorChanged, [this, &key](QColor color) {
            mStyle->SetMaterial(MapProperty(key), engine::detail::UIColor(ToGfx(color)));
            mPainter->DeleteMaterialInstanceByKey(MapProperty(key));
        });

        std::optional<gfx::Color4f> previous;

        if (const auto* material = mStyle->GetMaterialType(MapProperty(key)))
        {
            if (const auto* p = dynamic_cast<const engine::detail::UIColor*>(material))
            {
                previous = p->GetColor();

                dlg.setColor(FromGfx(previous.value()));
            }
        }
        if (dlg.exec() == QDialog::Rejected)
        {
            if (previous.has_value())
            {
                mStyle->SetMaterial(MapProperty(key), engine::detail::UIColor(previous.value()));
                mPainter->DeleteMaterialInstanceByKey(MapProperty(key));
            }
            else
            {
                mStyle->DeleteMaterial(MapProperty(key));
                mPainter->DeleteMaterialInstanceByKey(MapProperty(key));
            }
            return;
        }

        UpdateWidgetStyleString();

        ShowWidgetProperties();

        emit StyleEdited();
    }
}
void WidgetStyleWidget::SetMaterialGradient(const char* key)
{
    using Index = engine::detail::UIGradient::ColorIndex;

    if (mWidget)
    {
        std::optional<engine::detail::UIGradient> previous_gradient;

        DlgGradient dlg(this);

        connect(&dlg, &DlgGradient::GradientChanged, [this, &key](DlgGradient* dlg) {
            engine::detail::UIGradient gradient;
            gradient.SetColor(ToGfx(dlg->GetColor(0)), Index::GradientColor0);
            gradient.SetColor(ToGfx(dlg->GetColor(1)), Index::GradientColor1);
            gradient.SetColor(ToGfx(dlg->GetColor(2)), Index::GradientColor2);
            gradient.SetColor(ToGfx(dlg->GetColor(3)), Index::GradientColor3);
            gradient.SetGradient(dlg->GetGradientType());
            gradient.SetGamma(dlg->GetGamma());
            mStyle->SetMaterial(MapProperty(key), gradient);
            mPainter->DeleteMaterialInstanceByKey(MapProperty(key));
        });

        if (const auto* material = mStyle->GetMaterialType(MapProperty(key)))
        {
            if (const auto* p = dynamic_cast<const engine::detail::UIGradient*>(material))
            {
                const auto color_top_left = p->GetColor(Index::GradientColor0);
                const auto color_top_right = p->GetColor(Index::GradientColor1);
                const auto color_bot_left = p->GetColor(Index::GradientColor2);
                const auto color_bot_right = p->GetColor(Index::GradientColor3);

                previous_gradient = *p;

                dlg.SetColor(FromGfx(color_top_left), 0);
                dlg.SetColor(FromGfx(color_top_right), 1);
                dlg.SetColor(FromGfx(color_bot_left), 2);
                dlg.SetColor(FromGfx(color_bot_right), 3);
                dlg.SetGradientType(p->GetGradient());
                dlg.SetGamma(p->GetGamma());
            }
        }
        if (dlg.exec() == QDialog::Rejected)
        {
            if (previous_gradient.has_value())
            {
                mStyle->SetMaterial(MapProperty(key), previous_gradient.value());
                mPainter->DeleteMaterialInstanceByKey(MapProperty(key));
            }
            else
            {
                mStyle->DeleteMaterial(MapProperty(key));
                mPainter->DeleteMaterialInstanceByKey(MapProperty(key));
            }
            return;
        }

        UpdateWidgetStyleString();

        ShowWidgetProperties();

        emit StyleEdited();
    }
}

void WidgetStyleWidget::SetMaterialImage(const char* key)
{
    if (mWidget)
    {
        QString image_file = QFileDialog::getOpenFileName(this,
            tr("Select Image File"), "",
            tr("Images (*.png *.jpg *.jpeg)"));
        if (image_file.isEmpty())
            return;

        QString image_name;
        QString json_file;
        json_file = app::FindImageJsonFile(image_file);
        if (!json_file.isEmpty())
        {
            DlgImgView dlg(mParent);
            dlg.SetDialogMode();
            dlg.show();
            dlg.LoadImage(image_file);
            dlg.LoadJson(json_file);
            dlg.ResetTransform();
            if (dlg.exec() == QDialog::Rejected)
                return;
            image_file = dlg.GetImageFileName();
            json_file  = dlg.GetJsonFileName();
            image_name = dlg.GetImageName();
        }
        const auto& image_uri = mWorkspace->MapFileToWorkspace(image_file);
        const auto& json_uri  = mWorkspace->MapFileToWorkspace(json_file);

        // it's possible that json_uri is an empty string in which case the image file
        // is assumed to be a non-packed single image. UITexture class will handle this
        // case and will then skip trying to read the JSON file.
        engine::detail::UITexture texture;
        texture.SetTextureUri(image_uri);
        texture.SetMetafileUri(json_uri);
        texture.SetTextureName(app::ToUtf8(image_name));
        mStyle->SetMaterial(MapProperty(key), std::move(texture));

        mPainter->DeleteMaterialInstanceByKey(MapProperty(key));

        UpdateWidgetStyleString();

        ShowWidgetProperties();

        emit StyleEdited();
    }
}

void WidgetStyleWidget::SetWidget(uik::Widget* widget)
{
    mWidget = widget;

    ShowWidgetProperties();
}

std::string WidgetStyleWidget::MapProperty(std::string key) const
{
    if (!mWidget)
        return key;

    // map some more primitive property keys to more specific
    // property keys that are probably more like what the user
    // wants/expects to happen.

    if (mWidget->GetType() == uik::Widget::Type::PushButton)
    {
        if (key == "/background")
            key = "/button-background";
        else if (key == "/border")
            key = "/button-border";
    }
    else if (mWidget->GetType() == uik::Widget::Type::Slider)
    {
        if (key == "/background")
            key = "/slider-background";
    }
    else if (mWidget->GetType() == uik::Widget::Type::ProgressBar)
    {
        if (key == "/background")
            key = "/progress-bar-background";
    }
    else if (mWidget->GetType() == uik::Widget::Type::SpinBox)
    {
        if (key == "/background")
            key = "/text-edit-background";
        else if (key == "/border")
            key = "/text-edit-border";
        else if (key == "/text-color")
            key = "/edit-text-color";
        else if (key == "/text-size")
            key = "/edit-text-size";
        else if (key == "/text-font")
            key = "/edit-text-font";
    }
    return mWidget->GetId() + mSelector + key;
}

template<typename T>
void WidgetStyleWidget::SetMaterial(const std::string& key, T material)
{
    mStyle->SetMaterial(key, std::move(material));
    // purge old material instance and force the painter
    // to recreate the material instance so that the changes
    // take effect
    mPainter->DeleteMaterialInstanceByKey(key);
}

void WidgetStyleWidget::DeleteMaterial(const std::string& key)
{
    mStyle->DeleteMaterial(key);
    mPainter->DeleteMaterialInstanceByKey(key);
}

} // namespace
