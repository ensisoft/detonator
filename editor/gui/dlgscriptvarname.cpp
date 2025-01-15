

#include "editor/app/eventlog.h"

#include "editor/gui/dlgscriptvarname.h"

namespace gui
{
void DlgScriptVarName::UpdateExample(QString name)
{
    QString str(R"(
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR/REC-html40/strict.dtd">
<html><head><meta name="qrichtext" content="1" /><style type="text/css">
p, li { white-space: pre-wrap; }
</style></head><body style=" font-family:'Sans Serif'; font-size:7pt; font-weight:400; font-style:normal;">
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
<span style=" font-size:10pt; font-weight:600; color:#c23528;">function</span><span style=" font-size:10pt;"> </span>
<span style=" font-size:10pt; font-style:italic;">Update</span><span style=" font-size:10pt;">(XXX, game_time, dt)</span></p>
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
<span style=" font-size:10pt;">  </span><span style=" font-size:10pt; color:#37be11;">-- your code here</span></p>
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
<span style=" font-size:10pt; font-weight:600; color:#c23528;">end</span></p></body></html>
            )");

    mUI.example->setText(str.replace("XXX", name));
}

void DlgScriptVarName::on_name_textEdited(const QString& text)
{
    if (text.isEmpty())
    {
        UpdateExample(mUI.name->placeholderText());
        return;
    }

    const auto backup = mUI.name->placeholderText();
    const auto pos = mUI.name->cursorPosition();
    const auto str = app::GenerateScriptVarName(text, backup);

    VERBOSE("script var name %1 => %2", text, str);

    if (str != text)
    {
        SetValue(mUI.name, str);
        mUI.name->setCursorPosition(str.size());
    }
    UpdateExample(mUI.name->text());
}

}// namespace