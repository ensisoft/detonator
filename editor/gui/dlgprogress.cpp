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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QEventLoop>
#  include <QMovie>
#  include "ui_dlgprogress.h"
#include "warnpop.h"

#include "base/utility.h"
#include "base/math.h"
#include "editor/app/format.h"
#include "editor/gui/dlgprogress.h"

namespace gui
{

DlgProgress::DlgProgress(QWidget* parent) noexcept
  : QDialog(parent)
{
    mUI = new Ui::DlgProgress;
    mUI->setupUi(this);
    //mUI->title->setText(APP_TITLE);
    QMovie* movie = new QMovie(this);
    movie->setFileName(":about.gif");
    movie->setSpeed(200);
    movie->start();
    //mUI->label->setMovie(movie);
}

DlgProgress::~DlgProgress() noexcept
{
    delete mUI;
}

void DlgProgress::SetMaximum(unsigned max) noexcept
{
    mUI->progressBar->setMaximum(max);
}
void DlgProgress::SetMinimum(unsigned min) noexcept
{
    mUI->progressBar->setMinimum(min);
}
void DlgProgress::UpdateState() noexcept
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto& update :  mUpdateQueue)
    {
        //if (auto* ptr = std::get_if<struct SetMessage>(&update))
            //mUI->message->setText(GetMessage(ptr->msg));
        if (auto* ptr = std::get_if<struct SetValue>(&update))
            mUI->progressBar->setValue(ptr->value);
        else if (auto* ptr = std::get_if<struct StepOne>(&update))
            mUI->progressBar->setValue(mUI->progressBar->value() + 1);
    }
    mUpdateQueue.clear();
}

QString DlgProgress::GetMessage(QString msg) const
{
    if (mSeriousness == Seriousness::VerySerious)
        return msg;

    static const QString alternatives[] = {
        "Slaying some dragons...",
        "On the way rescue the princess!",
        "Fighting the zombies!",
        "Teaching unicorns to tap dance...",
        "Convincing pixels to align perfectly...",
        "Summoning magical sprites from the digital realm...",
        "Training ninja turtles for a secret mission in the game world...",
        "Collecting golden coins for the pixelated piggy bank...",
        "Rescuing princesses from pixelated towers...",
        "Polishing magic wands for the ultimate spell-casting effect...",
        "Gathering fire flowers for an explosive entrance...",
        "Herding virtual cats through fantastical mazes...",
        "Taming wild polygons to behave in an orderly fashion...",
        "Growing pixelated mushrooms for extra life boosts...",
        "Assembling an army of pixel knights for an epic showdown..."
    };
    const unsigned max = base::ArraySize(alternatives);
    const unsigned min = 0;
    return alternatives[math::rand(min, max-1)];
}

} // namespace
