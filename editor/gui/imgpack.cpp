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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/json.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/gui/imgpack.h"

namespace gui
{

bool ReadImagePack(const QString& file, ImagePack* pack)
{
    auto& images = pack->images;

    QString err_str;
    QFile::FileError err_val = QFile::FileError::NoError;
    const auto& buff = app::ReadBinaryFile(file, &err_val, &err_str);
    if (err_val != QFile::FileError::NoError)
    {
        ERROR("Failed to read file. [file='%1', error='%2']", file, err_str);
        return false;
    }

    const auto* beg  = buff.data();
    const auto* end  = buff.data() + buff.size();
    const auto& json = nlohmann::json::parse(beg, end, nullptr, false);
    if (json.is_discarded())
    {
        ERROR("Failed to parse JSON file. [file='%1']", file);
        return false;
    }

    std::string image_file;
    std::string app_name;
    std::string app_version;
    base::JsonReadSafe(json, "made_with_app",     &app_name);
    base::JsonReadSafe(json, "made_with_ver",     &app_version);
    base::JsonReadSafe(json, "json_version",      &pack->version);
    base::JsonReadSafe(json, "image_width",       &pack->image_width);
    base::JsonReadSafe(json, "image_height",      &pack->image_height);
    base::JsonReadSafe(json, "image_file",        &image_file);
    base::JsonReadSafe(json, "padding",           &pack->padding);
    base::JsonReadSafe(json, "color_space",       &pack->color_space);
    base::JsonReadSafe(json, "min_filter",        &pack->min_filter);
    base::JsonReadSafe(json, "mag_filter",        &pack->mag_filter);
    base::JsonReadSafe(json, "premultiply_alpha", &pack->premultiply_alpha_hint);
    base::JsonReadSafe(json, "premulalpha_blend", &pack->premultiply_blend_hint);
    base::JsonReadSafe(json, "power_of_two",      &pack->power_of_two_hint);
    pack->app_version = app::FromUtf8(app_version);
    pack->app_name    = app::FromUtf8(app_name);
    pack->image_file  = app::FromUtf8(image_file);

    if (json.contains("images") && json["images"].is_array())
    {
        bool mega_error =false;
        for (const auto& img_json: json["images"].items())
        {
            const auto& obj = img_json.value();

            ImagePack::Image img;
            std::string name;
            std::string character;
            std::string tag;

            // all these are optionally in the JSON
            base::JsonReadSafe(obj, "name",  &name);
            base::JsonReadSafe(obj, "char",  &character);
            base::JsonReadSafe(obj, "tag",   &tag);
            base::JsonReadSafe(obj, "index", &img.index);

            img.name      = app::FromUtf8(name);
            img.character = app::FromUtf8(character);
            img.tag       = app::FromUtf8(tag);

            bool error = true;
            if (!base::JsonReadSafe(obj, "width", &img.width))
                WARN("Image is missing 'width' attribute. [image='%1']", name);
            if (!base::JsonReadSafe(obj, "height", &img.height))
                WARN("Image is missing 'height' attribute. [image='%1']", name);
            if (!base::JsonReadSafe(obj, "xpos", &img.xpos))
                WARN("Image is missing 'xpos' attribute. [image='%1']", name);
            if (!base::JsonReadSafe(obj, "ypos", &img.ypos))
                WARN("Image is missing 'ypos' attribute. [image='%1']", name);
            else error = false;

            if (error)
            {
                mega_error = true;
                continue;
            }

            images.push_back(std::move(img));
        }
        if (mega_error)
        {
            WARN("Problems were detected while reading image pack file. [file='%1']", file);
        }
    }
    else
    {
        unsigned image_width  = 0;
        unsigned image_height = 0;
        unsigned tile_width  = 0;
        unsigned tile_height = 0;
        unsigned xoffset = 0;
        unsigned yoffset = 0;
        bool error = true;

        if (!base::JsonReadSafe(json, "image_width", &image_width))
            ERROR("Missing image_width property. [file='%1']", file);
        else if (!base::JsonReadSafe(json, "image_height", &image_height))
            ERROR("Missing image_height property. [file='%1']", file);
        else if (!base::JsonReadSafe(json, "tile_width", &tile_width))
            ERROR("Missing tile_width property. [file='%1']", file);
        else if (!base::JsonReadSafe(json, "tile_height", &tile_height))
            ERROR("Missing tile_height property. [file='%1']", file);
        else if (!base::JsonReadSafe(json, "xoffset", &xoffset))
            ERROR("Missing xoffset property.[file='%1']", file);
        else if (!base::JsonReadSafe(json, "yoffset", &yoffset))
            ERROR("Missing yoffset property. [file='%1']", file);
        else error = false;

        if (error)
            return false;

        const auto max_rows = (image_height - yoffset) / tile_height;
        const auto max_cols = (image_width - xoffset) / tile_width;
        for (unsigned row=0; row<max_rows; ++row)
        {
            for (unsigned col=0; col<max_cols; ++col)
            {
                const auto index = row * max_cols + col;
                const auto tile_xpos = xoffset + col * tile_width;
                const auto tile_ypos = yoffset + row * tile_height;

                ImagePack::Image img;
                img.width  = tile_width;
                img.height = tile_height;
                img.xpos   = tile_xpos;
                img.ypos   = tile_ypos;
                images.push_back(std::move(img));
            }
        }
        ImagePack::Tilemap map;
        map.tile_width  = tile_width;
        map.tile_height = tile_height;
        map.xoffset     = xoffset;
        map.yoffset     = yoffset;
        pack->tilemap   = map;
    }

    if (json.contains("tags") && json["tags"].is_array())
    {
        const auto& tags = json["tags"];
        for (unsigned i=0; i<tags.size(); ++i)
        {
            std::string tag;
            if (base::JsonReadSafe(tags[i], &tag))
            {
                if (i < images.size())
                    images[i].tag = app::FromUtf8(tag);
            }
        }
    }

    // finally, sort based on the image index.
    std::stable_sort(std::begin(images), std::end(images), [&](const auto& a, const auto& b) {
        return a.index < b.index;
    });

    INFO("Found %1 images in '%2'.", images.size(), file);
    return true;
}

bool WriteImagePack(const QString& file, const ImagePack& pack)
{
    nlohmann::json json;
    base::JsonWrite(json, "made_with_app",     APP_TITLE);
    base::JsonWrite(json, "made_with_ver",     APP_VERSION);
    base::JsonWrite(json, "json_version",      1);
    base::JsonWrite(json, "image_file",        app::ToUtf8(pack.image_file));
    base::JsonWrite(json, "padding",           pack.padding);
    base::JsonWrite(json, "image_width",       pack.image_width);
    base::JsonWrite(json, "image_height",      pack.image_height);
    base::JsonWrite(json, "color_space",       pack.color_space);
    base::JsonWrite(json, "min_filter",        pack.min_filter);
    base::JsonWrite(json, "mag_filter",        pack.mag_filter);
    base::JsonWrite(json, "premultiply_alpha", pack.premultiply_alpha_hint);
    base::JsonWrite(json, "premulalpha_blend", pack.premultiply_blend_hint);
    base::JsonWrite(json, "power_of_two",      pack.power_of_two_hint);

    if (pack.tilemap.has_value())
    {
        const auto& map = pack.tilemap.value();
        base::JsonWrite(json, "tile_width",    map.tile_width);
        base::JsonWrite(json, "tile_height",   map.tile_height);
        base::JsonWrite(json, "xoffset",       map.xoffset);
        base::JsonWrite(json, "yoffset",       map.yoffset);

        bool has_tags = false;
        for (const auto& img : pack.images)
        {
            if (!img.tag.isEmpty())
            {
                has_tags = true;
                break;
            }
        }
        if (has_tags)
        {
            for (const auto& img : pack.images)
            {
                json["tags"].push_back(app::ToUtf8(img.tag));
            }
        }
    }
    else
    {
        for (const auto& img : pack.images)
        {
            nlohmann::json img_json;
            base::JsonWrite(img_json, "width",  img.width);
            base::JsonWrite(img_json, "height", img.height);
            base::JsonWrite(img_json, "xpos",   img.xpos);
            base::JsonWrite(img_json, "ypos",   img.ypos);
            base::JsonWrite(img_json, "index",  img.index);
            // these are optional, if there's no value then skp the json
            if (!img.character.isEmpty())
                base::JsonWrite(img_json, "char", app::ToUtf8(img.character));
            if (!img.name.isEmpty())
                base::JsonWrite(img_json, "name", app::ToUtf8(img.name));
            if (!img.tag.isEmpty())
                base::JsonWrite(img_json, "tag", app::ToUtf8(img.tag));

            json["images"].push_back(std::move(img_json));
        }
    }

    QString err_str;
    QFile::FileError err_val = QFile::FileError::NoError;
    if (!app::WriteTextFile(file, json.dump(2), &err_val, &err_str))
    {
        ERROR("File write error. [file='%1', error='%2']", file, err_str);
        return false;
    }
    DEBUG("Wrote image pack JSON file. [file='%1']", file);
    INFO("Wrote image pack JSON to '%1'.", file);
    return true;
}

} // namespace
