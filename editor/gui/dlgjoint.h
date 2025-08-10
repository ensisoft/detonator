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
#  include "ui_dlgjoint.h"
#  include <QDialog>
#include "warnpop.h"

#include <memory>

#include "game/fwd.h"

namespace gui
{
    class DlgJoint : public QDialog
    {
        Q_OBJECT

    public:
        DlgJoint(QWidget* parent, const game::EntityClass& klass, game::RigidBodyJointClass& joint);
       ~DlgJoint() override;

    private slots:
        void on_btnSwap_clicked();
        void on_btnApply_clicked();
        void on_btnAccept_clicked();
        void on_btnCancel_clicked();
        void on_btnResetSrcAnchor_clicked();
        void on_btnResetDstAnchor_clicked();
        void on_btnResetMinDistance_clicked();
        void on_btnResetMaxDistance_clicked();
        void on_cmbType_currentIndexChanged(int);
        void on_srcX_valueChanged(double);
        void on_srcY_valueChanged(double);
        void on_dstX_valueChanged(double);
        void on_dstY_valueChanged(double);
        void on_dirAngle_valueChanged(double);

    private:
        bool Apply();
        void Show();

    private:
        Ui::DlgJoint mUI;
    private:
        const game::EntityClass& mEntity;
        game::RigidBodyJointClass& mJoint;
    };
} // namespace
