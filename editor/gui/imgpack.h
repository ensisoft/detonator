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
#  include <QString>
#include "warnpop.h"

#include <optional>
#include <vector>
#include <string>

#include "graphics/material.h"

namespace gui
{
    class ImportedTile;

    struct ImagePack
    {
        QString app_version = APP_VERSION;
        QString app_name    = APP_TITLE;
        unsigned version    = 1;

        QString image_file;
        unsigned image_width = 0;
        unsigned image_height = 0;
        unsigned padding = 0;
        bool premultiply_alpha_hint = false;
        bool premultiply_blend_hint = false;
        bool power_of_two_hint      = false;
        gfx::detail::TextureFileSource::ColorSpace color_space = gfx::detail::TextureFileSource::ColorSpace::Linear;
        gfx::MaterialClass::MinTextureFilter min_filter = gfx::MaterialClass::MinTextureFilter::Default;
        gfx::MaterialClass::MagTextureFilter mag_filter = gfx::MaterialClass::MagTextureFilter::Default;

        struct Image {
            QString name;
            QString character;
            QString tag;
            unsigned xpos   = 0;
            unsigned ypos   = 0;
            unsigned width  = 0;
            unsigned height = 0;
            unsigned index  = 0;
            bool selected   = false;
            ImportedTile* widget = nullptr;
        };
        struct Tilemap {
            unsigned tile_width  = 0;
            unsigned tile_height = 0;
            unsigned xoffset = 0;
            unsigned yoffset = 0;
        };
        std::optional<Tilemap> tilemap;
        std::vector<Image> images;
    };

    bool ReadImagePack(const QString& file, ImagePack* pack);
    bool WriteImagePack(const QString& file, const ImagePack& pack);

} // namespace
