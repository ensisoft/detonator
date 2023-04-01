// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include <QtGlobal>

#include "base/assert.h"

#include "ui_collapsible_widget.h"
#include "collapsible_widget.h"

namespace gui
{

CollapsibleWidget::CollapsibleWidget(QWidget* parent)
  : QWidget(parent)
{
    mUI = std::make_unique<Ui::CollapsibleWidget>();
    mUI->setupUi(this);
    mUI->collapsible_widget_label->installEventFilter(this);
    setFocusPolicy(Qt::FocusPolicy::TabFocus);
}

CollapsibleWidget::~CollapsibleWidget() = default;

void CollapsibleWidget::Collapse(bool value)
{
    if (value)
    {
        mUI->collapsible_widget_stacked_widget->hide();
        mUI->collapsible_widget_button->setArrowType(Qt::ArrowType::RightArrow);
    }
    else
    {
        mUI->collapsible_widget_stacked_widget->show();
        mUI->collapsible_widget_button->setArrowType(Qt::ArrowType::DownArrow);
    }
    mCollapsed = value;
}

QString CollapsibleWidget::GetText() const
{
    return mUI->collapsible_widget_label->text();
}

void CollapsibleWidget::SetText(const QString& text)
{
    mUI->collapsible_widget_label->setText(text);
}

void CollapsibleWidget::AddPage(QWidget* page)
{
    page->setParent(mUI->collapsible_widget_stacked_widget);
    mUI->collapsible_widget_stacked_widget->insertWidget(mUI->collapsible_widget_stacked_widget->count(), page);
}

int CollapsibleWidget::GetCount() const
{
    return mUI->collapsible_widget_stacked_widget->count();
}

QWidget* CollapsibleWidget::GetWidget(int index)
{
    return mUI->collapsible_widget_stacked_widget->widget(index);
}

QFrame::Shape CollapsibleWidget::GetFrameShape() const
{
    return mUI->collapsible_widget_stacked_widget->frameShape();
}

void CollapsibleWidget::SetFrameShape(QFrame::Shape shape)
{
    mUI->collapsible_widget_stacked_widget->setFrameShape(shape);
}

QFrame::Shadow CollapsibleWidget::GetFrameShadow() const
{
    return mUI->collapsible_widget_stacked_widget->frameShadow();
}
void CollapsibleWidget::SetFrameShadow(QFrame::Shadow shadow)
{
    mUI->collapsible_widget_stacked_widget->setFrameShadow(shadow);
}

void CollapsibleWidget::on_collapsible_widget_button_clicked()
{
    Collapse(!mCollapsed);

    emit StateChanged(mCollapsed);
}

bool CollapsibleWidget::eventFilter(QObject* destination, QEvent* event)
{
    if (destination != mUI->collapsible_widget_label)
        return QWidget::eventFilter(destination, event);

    if (event->type() == QEvent::MouseButtonPress)
    {
        Collapse(!mCollapsed);
        return true;
    }
    return false;
}

void CollapsibleWidget::focusInEvent(QFocusEvent* ford)
{
    //qDebug("CollapsibleWidget::focusInEvent. widget=%s", qUtf8Printable(objectName()));

    mUI->collapsible_widget_button->setFocus();
    //QWidget::focusInEvent(ford); // probably not??
}

void CollapsibleWidget::focusOutEvent(QFocusEvent* ford)
{
    //qDebug("CollapsibleWidget::focusOutEvent. widget=%s", qUtf8Printable(objectName()));

    //QWidget::focusOutEvent(ford); // probably not??
}

bool CollapsibleWidget::focusNextPrevChild(bool next)
{
    //qDebug("CollapsibleWidget::focusNextPrevChild. widget=%s, go=%s", qUtf8Printable(objectName()), next ? "next" : "prev");

    const bool go_next = next;
    const bool go_prev = !next;

    auto* page_widget = mUI->collapsible_widget_stacked_widget->widget(0);
    std::vector<QWidget*> focus_list;
    auto* iterator = page_widget;
    while ((iterator = iterator->nextInFocusChain()) && iterator != page_widget)
    {
        auto* widget = iterator;
        if (widget->parent() != page_widget)
            continue;
        else if (!widget->isVisibleTo(page_widget))
            continue;
        else if (widget->focusPolicy() == Qt::NoFocus)
            continue;
        else if (!widget->isEnabled())
            continue;

        focus_list.push_back(widget);
        //qDebug("Found new focus list widget %s", qUtf8Printable(widget->objectName()));
    }
    if (focus_list.empty())
        return false;

    if (mUI->collapsible_widget_button->hasFocus())
    {
        //qDebug("button has focus");
        if (mCollapsed)
            return false;
        if (go_prev) {
            //mUI.collapsible_widget_button->clearFocus();
            return false;
        } else {
            auto* first = focus_list[0];
            //mUI.collapsible_widget_button->clearFocus();
            first->setFocus();
            return true;
        }
    }

    auto* focus_widget = page_widget->focusWidget();
    size_t focus_widget_index = 0;
    for (; focus_widget_index < focus_list.size(); ++focus_widget_index) {
        if (focus_list[focus_widget_index] == focus_widget)
            break;
    }
    ASSERT(focus_widget_index < focus_list.size());

    const auto first_index = focus_widget_index == 0;
    const auto last_index  = focus_widget_index == focus_list.size() -1;
    if (first_index && go_prev) {
        mUI->collapsible_widget_button->setFocus();
        return true;
    } else if (last_index && go_next) {
        //return QWidget::focusNextPrevChild(next);
        return false;
    }

    //qDebug("Currently focused widget %s", qUtf8Printable(focus_widget->objectName()));

    if (next) {
        auto* next_widget = focus_list[focus_widget_index+1];
        next_widget->setFocus();
        //qDebug("focus on next widget %s", qUtf8Printable(next_widget->objectName()));
    } else {
        auto* prev_widget = focus_list[focus_widget_index-1];
        prev_widget->setFocus();
        //qDebug("focus on prev widget %s", qUtf8Printable(prev_widget->objectName()));
    }
    return true;
}

} // namespace

