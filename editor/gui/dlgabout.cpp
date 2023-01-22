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
#  include <QFontDatabase>
#  include <QMovie>
#include "warnpop.h"

#include "editor/app/format.h"
#include "editor/gui/dlgabout.h"

//#include "git.h"
extern "C" const char* git_CommitSHA1();
extern "C" const char* git_Branch();

namespace gui
{

DlgAbout::DlgAbout(QWidget* parent) : QDialog(parent)
{
    mUI.setupUi(this);
    setWindowTitle(APP_TITLE);
    mUI.title->setText(APP_TITLE);
    mUI.title->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    mUI.copyright->setText(APP_COPYRIGHT);
    mUI.copyright->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    //mUI.www->setText(APP_LINKS);
    //mUI.www->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    mUI.build->setText(app::toString("Branch: '%1'\nCommit: %2\nDate: '%3'\nCompiler: %4 %5\nRel: %6",
        git_Branch(), git_CommitSHA1(), __DATE__ ", " __TIME__, COMPILER_NAME, COMPILER_VERSION, APP_VERSION));
    mUI.build->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    QMovie* movie = new QMovie(this);
    movie->setFileName(":splash.gif");
    movie->start();
    mUI.animation->setMovie(movie);
}

} // namespace
