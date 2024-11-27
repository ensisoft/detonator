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
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    palette.setColor(QPalette::Base, QColor(42, 42, 42));
    palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    palette.setColor(QPalette::Text, QColor(255, 255, 255));
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

void DarkStyle::polish(QApplication *app)
{
    if (!app)
        return;

    const char* QSS = R"(
QToolTip{
  color:palette(text);
  background-color:palette(base);
  border:1px solid palette(highlight);
  border-radius:0px;
}
QStatusBar{
  background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  color:palette(mid);
}
QMenuBar{
  background-color:transparent;
}
QMenuBar::item{
  background:transparent;
}
QMenuBar::item:selected{
  background-color:rgba(80,80,80,255);
}
QMenuBar::item:pressed{
  background-color:palette(highlight);
  color:rgba(255,255,255,255);
  border-left:1px solid rgba(25,25,25,127);
  border-right:1px solid rgba(25,25,25,127);
}
QMenu{
  background-color:palette(window);
  border:1px solid palette(shadow);
}
QMenu::item{
  padding:3px 15px 3px 15px;
  border:1px solid transparent;
}
QMenu::item:disabled{
  background-color:rgba(35,35,35,127);
  color:palette(disabled);
}
QMenu::item:selected{
  border-color:rgba(147,191,236,127);
  background:palette(highlight);
  color:rgba(255,255,255,255);
}
QMenu::icon:checked{
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border:1px solid palette(highlight);
  border-radius:2px;
}
QMenu::separator{
  height:1px;
  background:palette(alternate-base);
  margin-left:5px;
  margin-right:5px;
}
QMenu::indicator{
  width:15px;
  height:15px;
}
QMenu::indicator:non-exclusive:checked{
  padding-left:2px;
}
QMenu::indicator:non-exclusive:unchecked{
  padding-left:2px;
}
QMenu::indicator:exclusive:checked{
  padding-left:2px;
}
QMenu::indicator:exclusive:unchecked{
  padding-left:2px;
}
QToolBar::top{
  background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border-bottom:3px solid qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
}
QToolBar::bottom{
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border-top:3px solid qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
}
QToolBar::left{
  background-color:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border-right:3px solid qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
}
QToolBar::right{
  background-color:qlineargradient(x1:1,y1:0,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border-left:3px solid qlineargradient(x1:1,y1:0,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
}
QMainWindow::separator{
  width:6px;
  height:5px;
  padding:2px;
}
QComboBox QAbstractItemView {
  border: 1px solid palette(shadow);
}
QComboBox:item:selected {
  border: none;
  background:palette(highlight);
  color:rgba(255,255,255,255);
}
QComboBox::indicator{
  background-color:transparent;
  selection-background-color:transparent;
  color:transparent;
  selection-color:transparent;
}

QSplitter::handle:horizontal{
  width:0px;
}
QSplitter::handle:vertical{
  height:0px;
}

QMainWindow::separator:hover,QSplitter::handle:hover{
  background:palette(highlight);
}
QDockWidget::title{
  padding:4px;
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border:1px solid rgba(25,25,25,75);
  border-bottom:2px solid rgba(25,25,25,75);
}
QDockWidget{
  titlebar-close-icon:url(:/darkstyle/icons/close.svg);
  titlebar-normal-icon:url(:/darkstyle/icons/restore.svg);
}
QDockWidget::close-button,QDockWidget::float-button{
  subcontrol-position:top right;
  subcontrol-origin:margin;
  position:absolute;
  top:3px;
  bottom:0px;
  width:20px;
  height:20px;
}
QDockWidget::close-button{
  right:3px;
}
QDockWidget::float-button{
  right:25px;
}
QGroupBox{
  background-color:rgba(66,66,66,50%);
  margin-top:27px;
  border:1px solid rgba(25,25,25,127);
  border-radius:4px;
}
QGroupBox::title{
  subcontrol-origin:margin;
  subcontrol-position:left top;
  padding:4px 6px;
  margin-left:3px;
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border:1px solid rgba(25,25,25,75);
  border-bottom:2px solid rgb(127,127,127);
  border-top-left-radius:4px;
  border-top-right-radius:4px;
}
QTabWidget::pane{
  background-color:rgba(66,66,66,50%);
  border-top:1px solid rgba(25,25,25,50%);
}
QTabWidget::tab-bar{
  left:3px;
  top:1px;
}
QTabBar{
  background-color:transparent;
  qproperty-drawBase:0;
  border-bottom:1px solid rgba(25,25,25,50%);
}
QTabBar::tab{
  padding:4px 6px;
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border:1px solid rgba(25,25,25,75);
  border-top-left-radius:4px;
  border-top-right-radius:4px;
}
QTabBar::tab:selected,QTabBar::tab:hover{
  background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(53,53,53,127),stop:1 rgba(66,66,66,50%));
  border-bottom-color:rgba(66,66,66,75%);
}
QTabBar::tab:selected{
  border-bottom:2px solid palette(highlight);
}
QTabBar::tab::selected:disabled{
  border-bottom:2px solid rgb(127,127,127);
}
QTabBar::tab:!selected{
  margin-top:2px;
}
QTabBar::close-button {
  border:1px solid transparent;
  border-radius:2px;
  image: url(:/darkstyle/icons/close.svg);
  padding: 6px;
}
QTabBar::close-button:hover {
  background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(106,106,106,255),stop:1 rgba(106,106,106,75));
  border:1px solid palette(base);
}
QTabBar::close-button:pressed {
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border:1px solid palette(base);
}
QCheckBox::indicator{
  width:18px;
  height:18px;
}
QRadioButton::indicator{
  width:18px;
  height:18px;
}
QTreeView, QTableView{
  alternate-background-color:palette(window);
  background:palette(base);
}
QTreeView QHeaderView::section, QTableView QHeaderView::section{
  background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
  border-style:none;
  border-bottom:1px solid palette(dark);
  padding-left:5px;
  padding-right:5px;
}
QTreeView::item:selected:disabled, QTableView::item:selected:disabled{
  background:rgb(80,80,80);
}
QTreeView::branch{
  background-color:palette(base);
}
QTreeView::branch:has-children:!has-siblings:closed,
QTreeView::branch:closed:has-children:has-siblings{
  border-image:none;
}
QTreeView::branch:open:has-children:!has-siblings,
QTreeView::branch:open:has-children:has-siblings{
  border-image:none;
}
QScrollBar:vertical{
  background:palette(base);
  border-top-right-radius:2px;
  border-bottom-right-radius:2px;
  width:16px;
  margin:0px;
}
QScrollBar::handle:vertical{
  background-color:palette(alternate-base);
  border-radius:2px;
  min-height:20px;
  margin:2px 4px 2px 4px;
}
QScrollBar::handle:vertical:hover{
  background-color:palette(highlight);
}
QScrollBar::add-line:vertical{
  background:none;
  height:0px;
  subcontrol-position:right;
  subcontrol-origin:margin;
}
QScrollBar::sub-line:vertical{
  background:none;
  height:0px;
  subcontrol-position:left;
  subcontrol-origin:margin;
}
QScrollBar:horizontal{
  background:palette(base);
  height:16px;
  margin:0px;
}
QScrollBar::handle:horizontal{
  background-color:palette(alternate-base);
  border-radius:2px;
  min-width:20px;
  margin:4px 2px 4px 2px;
}
QScrollBar::handle:horizontal:hover{
  background-color:palette(highlight);
}
QScrollBar::add-line:horizontal{
  background:none;
  width:0px;
  subcontrol-position:bottom;
  subcontrol-origin:margin;
}
QScrollBar::sub-line:horizontal{
  background:none;
  width:0px;
  subcontrol-position:top;
  subcontrol-origin:margin;
}
QSlider::handle:horizontal{
  border-radius:4px;
  border:1px solid rgba(25,25,25,255);
  background-color:palette(alternate-base);
  min-height:20px;
  margin:0 -4px;
}
QSlider::handle:horizontal:hover{
  background:palette(highlight);
}
QSlider::add-page:horizontal{
  background:palette(base);
}
QSlider::sub-page:horizontal{
  background:palette(highlight);
}
QSlider::sub-page:horizontal:disabled{
  background:rgb(80,80,80);
}
#mainHelpWidget {
    border-image: url(:/darkstyle/wave.png) 0 0 0 0 stretch stretch;
}
    )";

    app->setStyleSheet(QSS);
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
