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

#include "config.h"

#include "warnpush.h"
#  include <QTimer>
#  include <QDialog>
#include "warnpop.h"

#include <mutex>
#include <vector>
#include <variant>

namespace Ui {
    class DlgProgress;
}

namespace gui
{
    class DlgProgress : public QDialog
    {
        Q_OBJECT

    public:
        enum class Seriousness {
            VerySerious,
            NotSoSerious
        };

        explicit DlgProgress(QWidget* parent) noexcept;
        ~DlgProgress() noexcept;

        void SetMaximum(unsigned max) noexcept;
        void SetMinimum(unsigned min) noexcept;
        void UpdateState() noexcept;

        void SetSeriousness(Seriousness seriousness) noexcept
        { mSeriousness = seriousness; }

        void SetMessage(const QString& msg) noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            struct SetMessage set;
            set.msg = msg;
            mUpdateQueue.push_back(set);
        }
        void SetValue(unsigned value) noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            struct SetValue set;
            set.value = value;
            mUpdateQueue.push_back(set);

        }
        void StepOne() noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            struct StepOne step;
            mUpdateQueue.push_back(step);
        }
    private:
        QString GetMessage(QString msg) const;

    private:
        Ui::DlgProgress* mUI = nullptr;
    private:
        Seriousness mSeriousness = Seriousness::VerySerious;
    private:
        struct SetMessage {
            QString msg;
        };
        struct SetValue {
            unsigned value;
        };
        struct StepOne { };
        using Update = std::variant<struct SetMessage,
                struct SetValue, struct StepOne>;

        std::mutex mMutex;
        std::vector<Update> mUpdateQueue;
    };

} // namespace
