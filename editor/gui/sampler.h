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
#  include "ui_sampler.h"
#  include <QWidget>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include "editor/gui/utility.h"
#include "graphics/fwd.h"

namespace gui
{
    class Sampler : public QWidget
    {
        Q_OBJECT

    public:
        Sampler(QWidget* parent) : QWidget(parent)
        {
            mUI.setupUi(this);
            QMenu* menu  = new QMenu(this);
            QAction* add_texture_from_file = menu->addAction(QIcon("icons:folder.png"), "File");
            QAction* add_texture_from_text = menu->addAction(QIcon("icons:text.png"), "Text");
            QAction* add_texture_from_bitmap = menu->addAction(QIcon("icons:bitmap.png"), "Bitmap");
            connect(add_texture_from_file,   &QAction::triggered, this, &Sampler::AddNewTextureMapFromFile);
            connect(add_texture_from_text,   &QAction::triggered, this, &Sampler::AddNewTextureMapFromText);
            connect(add_texture_from_bitmap, &QAction::triggered, this, &Sampler::AddNewTextureMapFromBitmap);
            mUI.btnAddTextureMap->setMenu(menu);
            connect(mUI.btnDelTextureMap, &QPushButton::clicked, this, &Sampler::DelTextureMap);
            //connect(mUI.spriteFps, &QDoubleSpinBox::valueChanged, this, &Sampler::SpriteFpsValueChanged);
        }
        void ShowFps(bool on_off)
        {
            SetVisible(mUI.spriteFps, on_off);
        }
        void ShowLooping(bool on_off)
        {
            SetVisible(mUI.chkLooping, on_off);
        }
        void SetText(QString text)
        { SetValue(mUI.textureName, text); }
        void SetName(QString name)
        { mName = name; }
        QString GetName() const
        { return mName; }
        float GetSpriteFps() const
        { return GetValue(mUI.spriteFps); }
        void SetSpriteFps(float fps)
        { SetValue(mUI.spriteFps, fps); }
        void SetLooping(bool looping)
        { SetValue(mUI.chkLooping, looping); }
        bool IsLooping() const
        { return GetValue(mUI.chkLooping); }
    signals:
        void AddNewTextureMapFromFile();
        void AddNewTextureMapFromText();
        void AddNewTextureMapFromBitmap();
        void DelTextureMap();
        void SpriteFpsValueChanged(double value);
    private slots:
        void on_spriteFps_valueChanged(double value)
        { emit SpriteFpsValueChanged(value); };
    private:
        Ui::Sampler mUI;
    private:
        QString mName;
    };
} // namespace
