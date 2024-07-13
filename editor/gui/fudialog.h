// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "warnpush.h"
#  include <QDialog>
#  include <QString>
#  include <QEventLoop>
#  include <QMainWindow>
#include "warnpop.h"

namespace app {
    class Workspace;
} // app

namespace gui
{
    class FUWindowEventFilter;

    // The FUDialog (FU as in FUCK YOU) is meant to work around
    // the millions of Qt bugs that exist around opening dialogs
    // and restoring their state. (size and position)
    class FUDialog
    {
    public:
        explicit FUDialog(QWidget* parent);
        ~FUDialog();

        void LoadGeometry(const app::Workspace* workspace, const QString& key);
        void SaveGeometry(app::Workspace* workspace, const QString& key) const;

        int exec();
        void close();

        bool HandleEvent(QEvent* event);

    protected:
        void accept();
        void reject();
        void SetupFU(QWidget* widget);

    private:
        QWidget* mParent = nullptr;
        QMainWindow* mWindow = nullptr;
        FUWindowEventFilter* mWindowEventFilter = nullptr;
        QEventLoop mSatan;
        QByteArray mGeometry;
        bool mDidLoadGeometry = false;
    };

    class FUWindowEventFilter : public QObject
    {
        Q_OBJECT
    public:
        explicit FUWindowEventFilter(FUDialog* dialog) : mDialog(dialog)
        {}
        virtual bool eventFilter(QObject* receiver, QEvent* event) override
        {
            return mDialog->HandleEvent(event);
        }
    private:
        FUDialog* mDialog =nullptr;
    };
}