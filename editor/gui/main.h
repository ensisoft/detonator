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


#pragma once

#include "config.h"

#include "warnpush.h"
#  include <QApplication>
#  include <QObject>
#include "warnpop.h"

#include "editor/app/eventlog.h"

namespace gui {
    class Editor : public QApplication
    {
        Q_OBJECT

    public:
        Editor(int argc, char* argv[]) : QApplication(argc, argv)
        {}
        virtual bool notify(QObject* receiver, QEvent* e);
    public:
        static bool DebugEditor();
        static void SetEditorDebug(bool on_off);
    private:
        static bool mDebugEditor;
    };
} // namespace