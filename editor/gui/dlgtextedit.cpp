
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
#  include <QSyntaxHighlighter>
#  include <QRegularExpression>
#  include <QRegularExpressionMatch>
#  include <QRegularExpressionMatchIterator>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include "base/json.h"
#include "editor/app/eventlog.h"
#include "editor/gui/dlgtextedit.h"
#include "editor/app/utility.h"

namespace gui
{

class GLSLSyntax : public QSyntaxHighlighter {
public:
    explicit GLSLSyntax(QTextDocument* parent)
      : QSyntaxHighlighter(parent)
    {}
protected:
    virtual void highlightBlock(const QString& text) override
    {
        //VERBOSE("GLSL highlight block '%1'", text);

        enum class RuleType {
            Keyword, DataType, Builtin, Comment, Function
        };

        struct HighlightRule {
            QString pattern;
            RuleType type;
        };

        static HighlightRule rules[] = {
            {QStringLiteral("#version"),            RuleType::Keyword},
            {QStringLiteral("#define"),             RuleType::Keyword},
            {QStringLiteral("#ifdef"),              RuleType::Keyword},
            {QStringLiteral("#else"),               RuleType::Keyword},
            {QStringLiteral("#endif"),              RuleType::Keyword},
            {QStringLiteral("#ifndef"),             RuleType::Keyword},

            {QStringLiteral("\\bprecision\\b"),     RuleType::Keyword},
            {QStringLiteral("\\bmediump\\b"),       RuleType::Keyword},
            {QStringLiteral("\\bhighp\\b"),         RuleType::Keyword},
            {QStringLiteral("\\blowp\\b"),          RuleType::Keyword},
            {QStringLiteral("\\bif\\b"),            RuleType::Keyword},
            {QStringLiteral("\\belse\\b"),          RuleType::Keyword},
            {QStringLiteral("\\bdiscard\\b"),       RuleType::Keyword},
            {QStringLiteral("\\bfor\\b"),           RuleType::Keyword},
            {QStringLiteral("\\bbreak\\b"),         RuleType::Keyword},
            {QStringLiteral("\\bdo\\b"),            RuleType::Keyword},
            {QStringLiteral("\\bfalse\\b"),         RuleType::Keyword},
            {QStringLiteral("\\btrue\\b"),          RuleType::Keyword},
            {QStringLiteral("\\bvoid\\b"),          RuleType::Keyword},
            {QStringLiteral("\\buniform\\b"),       RuleType::Keyword},
            {QStringLiteral("\\battribute\\b"),     RuleType::Keyword},
            {QStringLiteral("\\bvarying\\b"),       RuleType::Keyword},
            {QStringLiteral("\\bstruct\\b"),        RuleType::Keyword},
            {QStringLiteral("\\bconst\\b"),         RuleType::Keyword},

            {QStringLiteral("\\bint\\b"),           RuleType::DataType},
            {QStringLiteral("\\bfloat\\b"),         RuleType::DataType},
            {QStringLiteral("\\bvec2\\b"),          RuleType::DataType},
            {QStringLiteral("\\bvec3\\b"),          RuleType::DataType},
            {QStringLiteral("\\bvec4\\b"),          RuleType::DataType},
            {QStringLiteral("\\bivec2\\b"),         RuleType::DataType},
            {QStringLiteral("\\bivec3\\b"),         RuleType::DataType},
            {QStringLiteral("\\bivec4\\b"),         RuleType::DataType},
            {QStringLiteral("\\bmat2\\b"),          RuleType::DataType},
            {QStringLiteral("\\bmat3\\b"),          RuleType::DataType},
            {QStringLiteral("\\bmat4\\b"),          RuleType::DataType},
            {QStringLiteral("\\bsampler2D\\b"),     RuleType::DataType},
            {QStringLiteral("\\bsampler3D\\b"),     RuleType::DataType},

            {QStringLiteral("\\bgl_FragCoord\\b"),  RuleType::Builtin},
            {QStringLiteral("\\bgl_FragColor\\b"),  RuleType::Builtin},
            {QStringLiteral("\\bgl_Position\\b"),   RuleType::Builtin},
            {QStringLiteral("\\bgl_PointSize\\b"),  RuleType::Builtin},

            // the order matters, we match by function first
            // but if the function is a built-in function the
            // match will override the function match.
            {QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"), RuleType::Function},

            {QStringLiteral("\\bmix\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bradians\\b"),       RuleType::Builtin},
            {QStringLiteral("\\degrees\\b"),        RuleType::Builtin},
            {QStringLiteral("\\bsin\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bcos\\b"),           RuleType::Builtin},
            {QStringLiteral("\\btan\\b"),           RuleType::Builtin},
            {QStringLiteral("\\basin\\b"),          RuleType::Builtin},
            {QStringLiteral("\\bacos\\b"),          RuleType::Builtin},
            {QStringLiteral("\\batan\\b"),          RuleType::Builtin},
            {QStringLiteral("\\bpow\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bexp\\b"),           RuleType::Builtin},
            {QStringLiteral("\\blog\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bexp2\\b"),          RuleType::Builtin},
            {QStringLiteral("\\blog2\\b"),          RuleType::Builtin},
            {QStringLiteral("\\bsqrt\\b"),          RuleType::Builtin},
            {QStringLiteral("\\binversesqrt\\b"),   RuleType::Builtin},
            {QStringLiteral("\\babs\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bsign\\b"),          RuleType::Builtin},
            {QStringLiteral("\\bfloor\\b"),         RuleType::Builtin},
            {QStringLiteral("\\bceil\\b"),          RuleType::Builtin},
            {QStringLiteral("\\bfract\\b"),         RuleType::Builtin},
            {QStringLiteral("\\bmod\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bmin\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bmax\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bclamp\\b"),         RuleType::Builtin},
            {QStringLiteral("\\bstep\\b"),          RuleType::Builtin},
            {QStringLiteral("\\bsmoothstep\\b"),    RuleType::Builtin},
            {QStringLiteral("\\blength\\b"),        RuleType::Builtin},
            {QStringLiteral("\\bdistance\\b"),      RuleType::Builtin},
            {QStringLiteral("\\bdot\\b"),           RuleType::Builtin},
            {QStringLiteral("\\bcross\\b"),         RuleType::Builtin},
            {QStringLiteral("\\bnormalize\\b"),     RuleType::Builtin},
            {QStringLiteral("\\bfaceforward\\b"),   RuleType::Builtin},
            {QStringLiteral("\\bnormalize\\b"),     RuleType::Builtin},
            {QStringLiteral("\\breflect\\b"),       RuleType::Builtin},
            {QStringLiteral("\\btexture2D\\b"),     RuleType::Builtin},

            {QStringLiteral("//[^\n]*"),            RuleType::Comment},
        };

        static const QColor Orange("#e87d3e");
        static const QColor Green("#008000");
        static const QColor Yellow("#808000");

        for (auto& rule : rules)
        {
            QRegularExpression regex(rule.pattern);
            QRegularExpressionMatchIterator it = regex.globalMatch(text);
            while (it.hasNext())
            {
                QRegularExpressionMatch match = it.next();
                const auto start  = match.capturedStart();
                const auto length = match.capturedLength();

                QTextCharFormat format;
                if (rule.type == RuleType::Keyword)
                {
                    format.setFontWeight(QFont::Bold);
                    format.setForeground(Orange);
                }
                else if (rule.type == RuleType::DataType)
                {
                    format.setFontWeight(QFont::Bold);
                    format.setForeground(Yellow);
                }
                else if (rule.type == RuleType::Builtin)
                {
                    //format.setFontItalic(true);
                    format.setFontWeight(QFont::Bold);
                    format.setForeground(Orange);
                }
                else if (rule.type == RuleType::Comment)
                {
                    format.setForeground(Green);
                }
                else if (rule.type == RuleType::Function)
                {
                    format.setFontItalic(true);
                    format.setForeground(Orange);
                }
                setFormat(start, length, format);
            }
        }

        static auto commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
        static auto commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));

        setCurrentBlockState(0);

        int startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(commentStartExpression);

        while (startIndex >= 0)
        {
            QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
            int endIndex = match.capturedStart();
            int commentLength = 0;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex
                                + match.capturedLength();
            }
            setFormat(startIndex, commentLength, Green);
            startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
        }
    }
private:

};


DlgTextEdit::DlgTextEdit(QWidget* parent) 
  : FUDialog(parent)
{

    auto* layout = new QPlainTextDocumentLayout(&mDocument);
    layout->setParent(this);
    mDocument.setDocumentLayout(layout);

    mUI.setupUi(this);

    SetupFU(this);
}

DlgTextEdit::~DlgTextEdit() noexcept
{
    delete mSyntaxHighlight;
}

void DlgTextEdit::SetTitle(const QString& str)
{
    setWindowTitle(str);
    mUI.groupBox->setTitle(str);
}

void DlgTextEdit::SetText(const app::AnyString& str, const std::string& format)
{
    app::AnyString kek = str;

    const QFont default_font;
    TextEditor::Settings settings;
    settings.highlight_current_line = false;
    settings.show_line_numbers      = false;
    settings.highlight_syntax       = false;
    settings.use_code_completer     = false;
    settings.font_description       = default_font.toString();
    settings.font_size              = default_font.pointSize();

    if (format == "JSON")
    {
        const auto& [ok, json, error] = base::JsonParse(str);
        if (ok)
        {
            kek = json.dump(2);
        }
    }
    else if (format == "GLSL")
    {
        QFont font;
        font.setFamily("Monospace");
        font.setFixedPitch(true);

        settings.font_description = font.toString();
        settings.font_size        = 8;
        settings.show_line_numbers = true;
        settings.highlight_current_line = true;
        settings.highlight_syntax = true;
        mSyntaxHighlight = new GLSLSyntax(&mDocument);
    }

    mUI.text->SetDocument(&mDocument);
    mUI.text->SetSettings(settings);
    mDocument.setPlainText(kek);
    mUI.text->ApplySettings();
}

app::AnyString DlgTextEdit::GetText() const
{
    return mDocument.toPlainText();
}

app::AnyString DlgTextEdit::GetText(const std::string& format) const
{
    auto ret = GetText();
    if (ret.IsEmpty())
        return ret;

    if (format == "JSON")
    {
        const auto& [ok, json, error] = base::JsonParse(ret);
        if (ok)
        {
            ret = json.dump();
        }
    }

    return ret;
}

void DlgTextEdit::on_btnAccept_clicked()
{
     accept();
}
void DlgTextEdit::on_btnCancel_clicked()
{
     reject();
}

} // namespace
