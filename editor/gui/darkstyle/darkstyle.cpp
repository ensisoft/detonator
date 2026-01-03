/*
The MIT License (MIT)

Copyright © 2018, Juergen Skrotzky (https://github.com/Jorgen-VikingGod, JorgenVikingGod@gmail.com)
Copyright © 2018-2024 Antonio Dias (https://github.com/antonypro)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "darkstyle.h"
#include "stylecommon.h"

DarkStyle::DarkStyle() : DarkStyle(styleBase())
{
#ifdef Q_OS_WIN
    m_hash_pixmap_cache[SP_MessageBoxInformation] = StyleCommon::winStandardPixmap(SP_MessageBoxInformation);
    m_hash_pixmap_cache[SP_MessageBoxWarning] = StyleCommon::winStandardPixmap(SP_MessageBoxWarning);
    m_hash_pixmap_cache[SP_MessageBoxCritical] = StyleCommon::winStandardPixmap(SP_MessageBoxCritical);
    m_hash_pixmap_cache[SP_MessageBoxQuestion] = StyleCommon::winStandardPixmap(SP_MessageBoxQuestion);
#endif
}

DarkStyle::DarkStyle(QStyle *style) : QProxyStyle(style)
{

}

DarkStyle::~DarkStyle()
{

}

QStyle *DarkStyle::styleBase() const
{
    QStyle *base = QStyleFactory::create(QStringLiteral("Fusion"));
    return base;
}

QIcon DarkStyle::standardIcon(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const
{
#ifdef Q_OS_WIN
    switch (standardPixmap)
    {
    case SP_MessageBoxInformation:
    case SP_MessageBoxWarning:
    case SP_MessageBoxCritical:
    case SP_MessageBoxQuestion:
    {
        QPixmap pixmap = m_hash_pixmap_cache.value(standardPixmap, QPixmap());

        if (!pixmap.isNull())
            return QIcon(pixmap);

        break;
    }
    default:
        break;
    }
#endif

    return QProxyStyle::standardIcon(standardPixmap, option, widget);
}

void DarkStyle::polish(QPalette &palette)
{
    QString style = property("style").toString();
    if (style == "Fusion-Dark")
    {
        palette.setColor(QPalette::Window, QColor(53, 53, 53));
        palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        palette.setColor(QPalette::Base, QColor(42, 42, 42));
        palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
        palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
        palette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
        palette.setColor(QPalette::Text, QColor(220, 220, 220));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        palette.setColor(QPalette::Dark, QColor(35, 35, 35));
        palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
        palette.setColor(QPalette::Button, QColor(53, 53, 53));
        palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    }
    else if (style == "DETONATOR")
    {
        palette.setColor(QPalette::Window, QColor("#3a3a3a"));
        palette.setColor(QPalette::Base, QColor("#232323"));
        palette.setColor(QPalette::Text, QColor(220, 220, 220));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#555"));
        palette.setColor(QPalette::Highlight, QColor("#b78620"));
        palette.setColor(QPalette::AlternateBase, QColor(183, 134, 32, 51));
    }
    else if (style == "DETONATOR2")
    {
        // ===== Base surfaces =====
        palette.setColor(QPalette::Window, QColor(15, 18, 24));        // #0f1218
        palette.setColor(QPalette::Base, QColor(11, 15, 22));          // #0b0f16 (inputs, views)
        palette.setColor(QPalette::AlternateBase, QColor(34, 39, 49)); // #0e131d
        palette.setColor(QPalette::ToolTipBase, QColor(28, 35, 48));   // #1c2330

        // ===== Text =====
        palette.setColor(QPalette::WindowText, QColor(233, 238, 247)); // #e9eef7
        palette.setColor(QPalette::Text, QColor(220, 230, 245));       // #dce6f5
        palette.setColor(QPalette::ToolTipText, QColor(233, 238, 247));

        // ===== Buttons =====
        palette.setColor(QPalette::Button, QColor(22, 29, 41));        // #161d29
        palette.setColor(QPalette::ButtonText, QColor(233, 238, 247));

        // ===== Bright text (warnings, errors) =====
        palette.setColor(QPalette::BrightText, QColor(255, 92, 122));  // error accent

        // ===== Selection / highlight =====
        palette.setColor(QPalette::Highlight, QColor(45, 212, 255));   // #2dd4ff
        palette.setColor(QPalette::HighlightedText, QColor(6, 16, 24));// #061018

        // ===== Links =====
        palette.setColor(QPalette::Link, QColor(45, 212, 255));
        palette.setColor(QPalette::LinkVisited, QColor(150, 120, 255));
    }
    else if (style == "Cyber Punk")
    {
        palette.setColor(QPalette::Window, QColor("#1e1e1e"));
        palette.setColor(QPalette::Base, QColor("#232323"));
        palette.setColor(QPalette::Text, QColor(220, 220, 220));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#555"));
        palette.setColor(QPalette::Highlight, QColor("#00c8ff"));
        palette.setColor(QPalette::AlternateBase, QColor(0, 200, 255, 51));
    }
}

void DarkStyle::polish(QApplication *app)
{
    if (!app)
        return;

    static const char darkstyle[] = {
#include "darkstyle.qss"
    };
    static const char detonator[] = {
#include "Detonator.qss"
    };
    static const char detonator2[] = {
#include "Detonator2.qss"
    };
    static const char cyber_punk[] = {
#include "cyberpunk.qss"
    };

    QString style = property("style").toString();
    if (style == "Fusion-Dark")
        app->setStyleSheet(darkstyle);
    else if (style == "DETONATOR")
        app->setStyleSheet(detonator);
    else if (style == "DETONATOR2")
        app->setStyleSheet(detonator2);
    else if (style == "Cyber Punk")
        app->setStyleSheet(cyber_punk);
}

void DarkStyle::unpolish(QApplication *app)
{
    if (!app)
        return;
}

QStyle *DarkStyle::baseStyle() const
{
    return styleBase();
}
