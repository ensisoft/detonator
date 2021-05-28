// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

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
#include "editor/app/workspace.h"
#include "editor/gui/widgetstylewidget.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/utility.h"

namespace gui
{
WidgetStyleWidget::WidgetStyleWidget(QWidget* parent) : QWidget(parent)
{
    mUI.setupUi(this);
    PopulateFromEnum<game::UIStyle::VerticalTextAlign>(mUI.widgetTextVAlign);
    PopulateFromEnum<game::UIStyle::HorizontalTextAlign>(mUI.widgetTextHAlign);

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
void WidgetStyleWidget::on_widgetTextColorEnable_stateChanged(int)
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
    SetValue(mUI.widgetTextColorEnable, true);
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
        if (mUI.widgetFontName->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/font-name");
        else mStyle->SetProperty(id + mSelector + "/font-name", (std::string)GetValue(mUI.widgetFontName));

        if (mUI.widgetFontSize->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/font-size");
        else mStyle->SetProperty(id + mSelector + "/font-size", (int)GetValue(mUI.widgetFontSize));

        if (mUI.widgetTextVAlign->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/text-vertical-align");
        else mStyle->SetProperty(id + mSelector + "/text-vertical-align",(game::UIStyle::VerticalTextAlign)GetValue(mUI.widgetTextVAlign));

        if (mUI.widgetTextHAlign->currentIndex() == -1)
            mStyle->DeleteProperty(id + mSelector + "/text-horizontal-align");
        else mStyle->SetProperty(id + mSelector + "/text-horizontal-align", (game::UIStyle::HorizontalTextAlign)GetValue(mUI.widgetTextHAlign));

        if (!GetValue(mUI.widgetTextColorEnable))
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
            mStyle->SetMaterial(id + mSelector + "/background", game::detail::UINullMaterial());
        else mStyle->SetMaterial(id + mSelector + "/background", game::detail::UIMaterialReference(GetItemId(mUI.widgetBackground)));

        if (mUI.widgetBorder->currentIndex() == -1)
            mStyle->DeleteMaterial(id + mSelector + "/border");
        else if (mUI.widgetBorder->currentIndex() == 0)
            mStyle->SetMaterial(id + mSelector + "/border", game::detail::UINullMaterial());
        else mStyle->SetMaterial(id + mSelector + "/border", game::detail::UIMaterialReference(GetItemId(mUI.widgetBorder)));

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
    SetValue(mUI.widgetTextColor, game::Color::White);
    SetValue(mUI.widgetTextColorEnable, false);
    SetValue(mUI.widgetTextBlink, Qt::PartiallyChecked);
    SetValue(mUI.widgetTextUnderline, Qt::PartiallyChecked);
    SetValue(mUI.widgetBackground, -1);
    SetValue(mUI.widgetBorder, -1);
    if (widget)
    {
        const auto& id = widget->GetId();

        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/font-name"))
            SetValue(mUI.widgetFontName, prop.GetValue<std::string>());
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/font-size"))
            SetValue(mUI.widgetFontSize, QString::number(prop.GetValue<int>()));

        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-vertical-align"))
            SetValue(mUI.widgetTextVAlign , prop.GetValue<game::UIStyle::VerticalTextAlign>());
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-horizontal-align"))
            SetValue(mUI.widgetTextHAlign , prop.GetValue<game::UIStyle::HorizontalTextAlign>());
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
        if (const auto& prop = mStyle->GetProperty(id + mSelector + "/text-color"))
        {
            SetValue(mUI.widgetTextColor , prop.GetValue<game::Color4f>());
            SetValue(mUI.widgetTextColorEnable , true);
        }

        if (const auto* material = mStyle->GetMaterialType(id + mSelector + "/background"))
        {
            if (material->GetType() == game::UIMaterial::Type::Null)
                SetValue(mUI.widgetBackground, QString("None"));
            else if (const auto* p = dynamic_cast<const game::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetBackground, ListItemId(p->GetMaterialId()));
        }
        if (const auto* material = mStyle->GetMaterialType(id + mSelector + "/border"))
        {
            if (material->GetType() == game::UIMaterial::Type::Null)
                SetValue(mUI.widgetBorder, QString("None"));
            else if (const auto* p = dynamic_cast<const game::detail::UIMaterialReference*>(material))
                SetValue(mUI.widgetBorder, ListItemId(p->GetMaterialId()));
        }
    }
    mWidget = widget;
}

} // namespace
