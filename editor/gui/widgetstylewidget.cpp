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
#include "warnpop.h"

#include "uikit/widget.h"
#include "engine/ui.h"
#include "engine/color.h"
#include "editor/app/workspace.h"
#include "editor/gui/widgetstylewidget.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"

namespace gui
{
WidgetStyleWidget::WidgetStyleWidget(QWidget* parent) : QWidget(parent)
{
    mUI.setupUi(this);
    PopulateFromEnum<engine::UIStyle::VerticalTextAlign>(mUI.widgetTextVAlign);
    PopulateFromEnum<engine::UIStyle::HorizontalTextAlign>(mUI.widgetTextHAlign);

    PopulateFontNames(mUI.widgetFontName);
    PopulateFontSizes(mUI.widgetFontSize);
}

void WidgetStyleWidget::RebuildMaterialCombos(const std::vector<ListItem>& list)
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
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetFontSize_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextVAlign_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextHAlign_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextBlink_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextUnderline_stateChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetTextColor_colorChanged(QColor color)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetBackground_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_widgetBorder_currentIndexChanged(int)
{
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetFontName_clicked()
{
    SetValue(mUI.widgetFontName, -1);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextColor_clicked()
{
    mUI.widgetTextColor->clearColor();
    UpdateCurrentWidgetProperties();
}

void WidgetStyleWidget::on_btnSelectWidgetFont_clicked()
{
    if (!mWidget)
        return;

    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Font File"), "", tr("Font (*.ttf *.otf)"));
    if (list.isEmpty())
        return;
    const auto& file = mWorkspace->MapFileToWorkspace(list[0]);
    SetValue(mUI.widgetFontName, file);
    UpdateCurrentWidgetProperties();
}

void WidgetStyleWidget::on_btnResetWidgetFontSize_clicked()
{
    SetValue(mUI.widgetFontSize, -1);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextVAlign_clicked()
{
    SetValue(mUI.widgetTextVAlign, -1);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextHAlign_clicked()
{
    SetValue(mUI.widgetTextHAlign, -1);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetTextProp_clicked()
{
    SetValue(mUI.widgetTextUnderline, Qt::PartiallyChecked);
    SetValue(mUI.widgetTextBlink, Qt::PartiallyChecked);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetBackground_clicked()
{
    SetValue(mUI.widgetBackground, -1);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnResetWidgetBorder_clicked()
{
    SetValue(mUI.widgetBorder, -1);
    UpdateCurrentWidgetProperties();
}
void WidgetStyleWidget::on_btnSelectWidgetBackground_clicked()
{
    DlgMaterial dlg(this, mWorkspace, GetItemId(mUI.widgetBackground));
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.widgetBackground, ListItemId(dlg.GetSelectedMaterialId()));
    UpdateCurrentWidgetProperties();

}
void WidgetStyleWidget::on_btnSelectWidgetBorder_clicked()
{
    DlgMaterial dlg(this, mWorkspace, GetItemId(mUI.widgetBorder));
    if (dlg.exec() == QDialog::Rejected)
        return;

    SetValue(mUI.widgetBorder, ListItemId(dlg.GetSelectedMaterialId()));
    UpdateCurrentWidgetProperties();
}

void WidgetStyleWidget::UpdateCurrentWidgetProperties()
{
    if (auto* widget = mWidget)
    {
        // set style properties
        const auto& id = widget->GetId();
        const std::string& font = GetValue(mUI.widgetFontName); // msvs build workaround
        if (font.empty())
            mStyle->DeleteProperty(id + mSelector + "/text-font");
        else mStyle->SetProperty(id + mSelector + "/text-font", font);

        if (mUI.widgetFontSize->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/text-size");
        else mStyle->SetProperty(id + mSelector + "/text-size", (int)GetValue(mUI.widgetFontSize));

        if (mUI.widgetTextVAlign->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/text-vertical-align");
        else mStyle->SetProperty(id + mSelector + "/text-vertical-align",(engine::UIStyle::VerticalTextAlign)GetValue(mUI.widgetTextVAlign));

        if (mUI.widgetTextHAlign->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/text-horizontal-align");
        else mStyle->SetProperty(id + mSelector + "/text-horizontal-align", (engine::UIStyle::HorizontalTextAlign)GetValue(mUI.widgetTextHAlign));

        if (!mUI.widgetTextColor->hasColor())
            mStyle->DeleteProperty(id + mSelector + "/text-color");
        else mStyle->SetProperty(id + mSelector + "/text-color", (gfx::Color4f)GetValue(mUI.widgetTextColor));

        const auto blink_state = mUI.widgetTextBlink->checkState();
        if (blink_state == Qt::PartiallyChecked)
            mStyle->DeleteProperty(id + mSelector + "/text-blink");
        else if (blink_state == Qt::Checked)
            mStyle->SetProperty(id + mSelector + "/text-blink", true);
        else if (blink_state == Qt::Unchecked)
            mStyle->SetProperty(id + mSelector + "/text-blink", false);

        const auto underline_state = mUI.widgetTextUnderline->checkState();
        if (underline_state == Qt::PartiallyChecked)
            mStyle->DeleteProperty(id + mSelector + "/text-underline");
        else if (underline_state == Qt::Checked)
            mStyle->SetProperty(id + mSelector + "/text-underline", true);
        else if (underline_state == Qt::Unchecked)
            mStyle->SetProperty(id + mSelector + "/text-underline", false);

        if (mUI.widgetBackground->currentIndex() == -1)
            mStyle->DeleteMaterial(id + mSelector + "/background");
        else if (mUI.widgetBackground->currentIndex() == 0)
            mStyle->SetMaterial(id + mSelector + "/background", engine::detail::UINullMaterial());
        else mStyle->SetMaterial(id + mSelector + "/background", engine::detail::UIMaterialReference(GetItemId(mUI.widgetBackground)));

        if (mUI.widgetBorder->currentIndex() == -1)
            mStyle->DeleteMaterial(id + mSelector + "/border");
        else if (mUI.widgetBorder->currentIndex() == 0)
            mStyle->SetMaterial(id + mSelector + "/border", engine::detail::UINullMaterial());
        else mStyle->SetMaterial(id + mSelector + "/border", engine::detail::UIMaterialReference(GetItemId(mUI.widgetBorder)));

        // purge old material instances if any.
        mPainter->DeleteMaterialInstanceByKey(id + mSelector + "/border");
        mPainter->DeleteMaterialInstanceByKey(id + mSelector + "/background");

        // update widget's style string so that it contains the
        // latest widget style.

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
        widget->SetStyleString(style);
    }
}

void WidgetStyleWidget::SetWidget(uik::Widget* widget)
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
    if (widget)
    {
        const auto& id = widget->GetId();

        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-font"))
            SetValue(mUI.widgetFontName, prop.GetValue<std::string>());
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-size"))
            SetValue(mUI.widgetFontSize, QString::number(prop.GetValue<int>()));

        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-vertical-align"))
            SetValue(mUI.widgetTextVAlign , prop.GetValue<engine::UIStyle::VerticalTextAlign>());
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-horizontal-align"))
            SetValue(mUI.widgetTextHAlign , prop.GetValue<engine::UIStyle::HorizontalTextAlign>());
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-color"))
            SetValue(mUI.widgetTextColor , prop.GetValue<engine::Color4f>());

        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-blink"))
        {
            const auto value = prop.GetValue<bool>();
            SetValue(mUI.widgetTextBlink , value ? Qt::Checked : Qt::Unchecked);
        }
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-underline"))
        {
            const auto value = prop.GetValue<bool>();
            SetValue(mUI.widgetTextUnderline , value ? Qt::Checked : Qt::Unchecked);
        }

        if (const auto* material = mStyle->GetMaterialType(id + mSelector + "/background"))
        {
            if (material->GetType() == engine::UIMaterial::Type::Null)
                SetValue(mUI.widgetBackground, QString("None"));
            else if (const auto* p = dynamic_cast<const engine::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetBackground, ListItemId(p->GetMaterialId()));
        }
        if (const auto* material = mStyle->GetMaterialType(id + mSelector + "/border"))
        {
            if (material->GetType() == engine::UIMaterial::Type::Null)
                SetValue(mUI.widgetBorder, QString("None"));
            else if (const auto* p = dynamic_cast<const engine::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetBorder, ListItemId(p->GetMaterialId()));
        }
    }
    mWidget = widget;
}

} // namespace
