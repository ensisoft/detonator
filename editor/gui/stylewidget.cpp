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
#  include <third_party/Qt-Color-Widgets/include/ColorDialog>
#include "warnpop.h"

#include "uikit/widget.h"
#include "engine/ui.h"
#include "editor/gui/dlggradient.h"
#include "editor/gui/stylewidget.h"
#include "editor/gui/utility.h"

namespace gui
{
StyleWidget::StyleWidget(QWidget* parent) : QWidget(parent)
{
    mUI.setupUi(this);

    // this is a smaller set than what is currently in engine/ui
    // because the style editor is not aware of Workspace thus it
    // cannot provide material references (actually it could do so
    // for the built-in materials but that's a TODO)
    // and currently it's difficult to figure out the right way
    // to map textures so they're left out too.
    enum class MaterialTypes {
        Null, Color, Gradient
    };

    PopulateFromEnum<game::UIStyle::WidgetShape>(mUI.widgetShape);
    PopulateFromEnum<game::UIStyle::VerticalTextAlign>(mUI.widgetTextVAlign);
    PopulateFromEnum<game::UIStyle::HorizontalTextAlign>(mUI.widgetTextHAlign);
    PopulateFromEnum<MaterialTypes>(mUI.widgetBackground);
    PopulateFromEnum<MaterialTypes>(mUI.widgetBorder);
    PopulateFontNames(mUI.widgetFontName);
    PopulateFontSizes(mUI.widgetFontSize);

    for (int i=0; i<25; ++i) {
        mUI.borderWidth->addItem(QString::number(i));
    }
}

void StyleWidget::on_widgetFontName_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetFontSize_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetTextVAlign_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetTextHAlign_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetTextColorEnable_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetTextBlink_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetTextUnderline_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetTextColor_colorChanged(QColor color)
{
    SetValue(mUI.widgetTextColorEnable, true);
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetBackground_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_widgetBorder_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_btnResetWidgetFontName_clicked()
{
    SetValue(mUI.widgetFontName, -1);
    UpdateCurrentWidgetProperties();
}

void StyleWidget::on_btnResetWidgetFontSize_clicked()
{
    SetValue(mUI.widgetFontSize, -1);
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_btnResetWidgetTextVAlign_clicked()
{
    SetValue(mUI.widgetTextVAlign, -1);
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_btnResetWidgetTextHAlign_clicked()
{
    SetValue(mUI.widgetTextHAlign, -1);
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_btnResetWidgetTextProp_clicked()
{
    SetValue(mUI.widgetTextUnderline, Qt::PartiallyChecked);
    SetValue(mUI.widgetTextBlink, Qt::PartiallyChecked);
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_btnResetWidgetBackground_clicked()
{
    SetValue(mUI.widgetBackground, -1);
    UpdateCurrentWidgetProperties();
}
void StyleWidget::on_btnResetWidgetBorder_clicked()
{
    SetValue(mUI.widgetBorder, -1);
    UpdateCurrentWidgetProperties();
}

void StyleWidget::UpdateCurrentWidgetProperties()
{
        // set style properties
    if (mUI.widgetShape->currentIndex() == -1)
        mStyle->DeleteProperty(mClass + mSelector + "/shape");
    else mStyle->SetProperty(mClass + mSelector + "/shape", (game::UIStyle::WidgetShape)GetValue(mUI.widgetShape));

    const std::string& font = GetValue(mUI.widgetFontName); // msvs build workaround
    if (font.empty())
        mStyle->DeleteProperty(mClass + mSelector + "/font-name");
    else mStyle->SetProperty(mClass + mSelector + "/font-name", font);

    if (mUI.widgetFontSize->currentIndex() == -1)
        mStyle->DeleteProperty(mClass + mSelector + "/font-size");
    else mStyle->SetProperty(mClass + mSelector + "/font-size", (int)GetValue(mUI.widgetFontSize));

    if (mUI.widgetTextVAlign->currentIndex() == -1)
        mStyle->DeleteProperty(mClass + mSelector + "/text-vertical-align");
    else mStyle->SetProperty(mClass+ mSelector + "/text-vertical-align",(game::UIStyle::VerticalTextAlign)GetValue(mUI.widgetTextVAlign));

    if (mUI.widgetTextHAlign->currentIndex() == -1)
        mStyle->DeleteProperty(mClass + mSelector + "/text-horizontal-align");
    else mStyle->SetProperty(mClass + mSelector + "/text-horizontal-align", (game::UIStyle::HorizontalTextAlign)GetValue(mUI.widgetTextHAlign));

    if (GetValue(mUI.widgetTextColorEnable))
        mStyle->DeleteProperty(mClass + mSelector + "/text-color");
    else mStyle->SetProperty(mClass + mSelector + "/text-color", (gfx::Color4f)GetValue(mUI.widgetTextColor));

    const auto blink_state = mUI.widgetTextBlink->checkState();
    if (blink_state == Qt::PartiallyChecked)
        mStyle->DeleteProperty(mClass + mSelector + "/text-blink");
    else if (blink_state == Qt::Checked)
        mStyle->SetProperty(mClass + mSelector + "/text-blink", true);
    else if (blink_state == Qt::Unchecked)
        mStyle->SetProperty(mClass + mSelector + "/text-blink", false);

    const auto underline_state = mUI.widgetTextUnderline->checkState();
    if (underline_state == Qt::PartiallyChecked)
        mStyle->DeleteProperty(mClass + mSelector + "/text-underline");
    else if (underline_state == Qt::Checked)
        mStyle->SetProperty(mClass + mSelector + "/text-underline", true);
    else if (underline_state == Qt::Unchecked)
        mStyle->SetProperty(mClass + mSelector + "/text-underline", false);

    if (mUI.widgetBackground->currentIndex() == -1)
        mStyle->DeleteMaterial(mClass + mSelector + "/background");
    else if (mUI.widgetBackground->currentIndex() == 0)
        mStyle->SetMaterial(mClass + mSelector + "/background", game::detail::UIColor(GetValue(mUI.backgroundColor0)));
    else if (mUI.widgetBackground->currentIndex() == 1)
        mStyle->SetMaterial(mClass + mSelector + "/background", game::detail::UIGradient(
                GetValue(mUI.backgroundColor0), GetValue(mUI.backgroundColor1),
                GetValue(mUI.backgroundColor2), GetValue(mUI.backgroundColor3)));

    if (mUI.widgetBorder->currentIndex() == -1)
        mStyle->DeleteMaterial(mClass + mSelector + "/border");
    else if (mUI.widgetBorder->currentIndex() == 0)
        mStyle->SetMaterial(mClass + mSelector + "/border", game::detail::UIColor(GetValue(mUI.borderColor0)));
    else if (mUI.widgetBorder->currentIndex() == 1)
        mStyle->SetMaterial(mClass + mSelector + "/border", game::detail::UIGradient(
                GetValue(mUI.borderColor0), GetValue(mUI.borderColor1),
                GetValue(mUI.borderColor2), GetValue(mUI.borderColor3)));
}

void StyleWidget::SetWidgetClass(const std::string& klass)
{
    SetValue(mUI.widgetShape, -1);
    SetValue(mUI.widgetFontName, -1);
    SetValue(mUI.widgetFontSize, -1);
    SetValue(mUI.widgetTextVAlign, -1);
    SetValue(mUI.widgetTextHAlign, -1);
    SetValue(mUI.widgetTextColor, game::Color::White);
    SetValue(mUI.widgetTextBlink, Qt::PartiallyChecked);
    SetValue(mUI.widgetTextUnderline, Qt::PartiallyChecked);
    SetValue(mUI.widgetBackground, -1);
    SetValue(mUI.widgetBorder, -1);

    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/shape"))
        SetValue(mUI.widgetShape, prop.GetValue<game::UIStyle::WidgetShape>());

    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/font-name"))
        SetValue(mUI.widgetFontName, prop.GetValue<std::string>());
    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/font-size"))
        SetValue(mUI.widgetFontSize, QString::number(prop.GetValue<int>()));

    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/text-vertical-align"))
        SetValue(mUI.widgetTextVAlign , prop.GetValue<game::UIStyle::VerticalTextAlign>());
    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/text-horizontal-align"))
        SetValue(mUI.widgetTextHAlign , prop.GetValue<game::UIStyle::HorizontalTextAlign>());
    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/text-blink"))
    {
        const auto value = prop.GetValue<bool>();
        SetValue(mUI.widgetTextBlink , value ? Qt::Checked : Qt::Unchecked);
    }
    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/text-underline"))
    {
        const auto value = prop.GetValue<bool>();
        SetValue(mUI.widgetTextUnderline , value ? Qt::Checked : Qt::Unchecked);
    }
    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/text-color"))
    {
        SetValue(mUI.widgetTextColor , prop.GetValue<game::Color4f>());
    }
    if (const auto* material = mStyle->GetMaterialType(klass + mSelector + "/background"))
    {
        SetValue(mUI.widgetBackground, material->GetType());
        if (const auto* m = dynamic_cast<const game::detail::UIColor*>(material))
        {
            mUI.backgroundColor0->setColor(FromGfx(m->GetColor()));
            mUI.backgroundColor1->setColor(FromGfx(m->GetColor()));
            mUI.backgroundColor2->setColor(FromGfx(m->GetColor()));
            mUI.backgroundColor3->setColor(FromGfx(m->GetColor()));
        }
        else if (const auto* m = dynamic_cast<const game::detail::UIGradient*>(material))
        {
            mUI.backgroundColor0->setColor(FromGfx(m->GetColor(0u)));
            mUI.backgroundColor1->setColor(FromGfx(m->GetColor(1u)));
            mUI.backgroundColor2->setColor(FromGfx(m->GetColor(2u)));
            mUI.backgroundColor3->setColor(FromGfx(m->GetColor(3u)));
        }
    }
    if (const auto* material = mStyle->GetMaterialType(klass + mSelector + "/border"))
    {
        SetValue(mUI.widgetBorder, material->GetType());
        if (const auto* m = dynamic_cast<const game::detail::UIColor*>(material))
        {
            mUI.borderColor0->setColor(FromGfx(m->GetColor()));
            mUI.borderColor1->setColor(FromGfx(m->GetColor()));
            mUI.borderColor2->setColor(FromGfx(m->GetColor()));
            mUI.borderColor3->setColor(FromGfx(m->GetColor()));
        }
        else if (const auto* m = dynamic_cast<const game::detail::UIGradient*>(material))
        {
            mUI.borderColor0->setColor(FromGfx(m->GetColor(0u)));
            mUI.borderColor1->setColor(FromGfx(m->GetColor(1u)));
            mUI.borderColor2->setColor(FromGfx(m->GetColor(2u)));
            mUI.borderColor3->setColor(FromGfx(m->GetColor(3u)));
        }
    }

    if (const auto& prop = mStyle->GetProperty(klass + mSelector + "/border-width"))
        SetValue(mUI.borderWidth, prop.GetValue<float>());

    mClass = klass;
}

} // namespace
