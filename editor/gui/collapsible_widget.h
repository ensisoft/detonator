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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QWidget>
#  include <QFrame>
#  include <QtUiPlugin/QDesignerExportWidget>
#include "warnpop.h"

#include <memory>

namespace Ui {
    class CollapsibleWidget;
}

namespace gui
{
    class DESIGNER_PLUGIN_EXPORT CollapsibleWidget : public QWidget
    {
        Q_OBJECT
        Q_PROPERTY(bool collapsed READ IsCollapsed WRITE Collapse DESIGNABLE true)
        Q_PROPERTY(QString text READ GetText WRITE SetText DESIGNABLE true)
        Q_PROPERTY(QFrame::Shape frameShape READ GetFrameShape WRITE SetFrameShape DESIGNABLE true)
        Q_PROPERTY(QFrame::Shadow frameShadow READ GetFrameShadow WRITE SetFrameShadow DESIGNABLE true);
    public:
        explicit CollapsibleWidget(QWidget* parent = nullptr);
        ~CollapsibleWidget();

        bool IsCollapsed() const
        { return mCollapsed; }
        void Collapse(bool value);
        QString GetText() const;
        void SetText(const QString& text);

        void AddPage(QWidget* page);
        void addPage(QWidget* page)
        { AddPage(page); }
        int GetCount() const;
        QWidget* GetWidget(int index);

        QFrame::Shape GetFrameShape() const;
        void SetFrameShape(QFrame::Shape shape);

        QFrame::Shadow GetFrameShadow() const;
        void SetFrameShadow(QFrame::Shadow shadow);

    signals:
        void StateChanged(bool collapsed);

    protected:
        virtual bool eventFilter(QObject* destination, QEvent* event) override;
        virtual void focusInEvent(QFocusEvent* ford) override;
        virtual void focusOutEvent(QFocusEvent* ford) override;
        virtual bool focusNextPrevChild(bool next) override;

    private slots:
        void on_collapsible_widget_button_clicked();

    private:
        std::unique_ptr<Ui::CollapsibleWidget> mUI;
    private:
        bool mCollapsed = false;
        bool mButtonFocus = false;
    };
} // namespace
