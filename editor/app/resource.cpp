// Copyright (C) 2020-2022 Sami Väisänen
// Copyright (C) 2020-2022 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "warnpush.h"
#  include <boost/algorithm/string/erase.hpp>
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <unordered_map>

#include "base/assert.h"
#include "engine/ui.h"
#include "editor/app/buffer.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/resource.h"

namespace {
    void PushBack(QStringList& list, const QString& id)
    {
        if (!id.isEmpty())
            list.push_back(id);
    }
    void PushBack(QStringList& list, const std::string& id)
    {
        if (!id.empty())
            list.push_back(app::FromUtf8(id));
    }
} // namespace

namespace app {
namespace detail {
QVariantMap DuplicateResourceProperties(const game::EntityClass& src, const game::EntityClass& dupe, const QVariantMap& props)
{
    ASSERT(src.GetNumNodes() == dupe.GetNumNodes());
    ASSERT(src.GetNumAnimations() == dupe.GetNumAnimations());

    QVariantMap ret = props;

    std::unordered_map<std::string, std::string> node_id_map;
    for (size_t i=0; i<src.GetNumNodes(); ++i)
    {
        const auto& src_node = src.GetNode(i);
        const auto& dst_node = dupe.GetNode(i);
        node_id_map[src_node.GetId()] = dst_node.GetId();
    }

    // remap node comments
    for (size_t i=0; i<src.GetNumNodes(); ++i)
    {
        const auto& src_node = src.GetNode(i);
        const auto& dst_node = dupe.GetNode(i);
        const auto& src_id   = FromUtf8(src_node.GetId());
        const auto& dst_id   = FromUtf8(dst_node.GetId());
        const auto& variant  = ret.value("comment_" + src_id);
        if (variant.isNull())
            continue;

        ret["comment_" + dst_id] = variant.toString();
        ret.remove("comment_" + src_id);
    }

    // remap per animation track properties
    for (size_t i=0; i<src.GetNumAnimations(); ++i)
    {
        const auto& src_track = src.GetAnimation(i);
        const auto& dst_track = dupe.GetAnimation(i);
        const auto& src_id    = FromUtf8(src_track.GetId());
        const auto& dst_id    = FromUtf8(dst_track.GetId());
        const auto& variant   = ret.value("track_" + src_id);
        if (variant.isNull())
            continue;

        ASSERT(src_track.GetNumActuators() == dst_track.GetNumActuators());

        std::unordered_map<std::string, std::string> actuator_id_map;
        for (size_t i=0; i<src_track.GetNumActuators(); ++i)
        {
            const auto& src_actuator = src_track.GetActuatorClass(i);
            const auto& dst_actuator = dst_track.GetActuatorClass(i);
            actuator_id_map[src_actuator.GetId()] = dst_actuator.GetId();
        }

        std::unordered_map<std::string, std::string> timeline_id_map;

        QVariantMap new_properties;
        QVariantMap old_properties = variant.toMap();
        const auto num_timelines = old_properties["num_timelines"].toUInt();
        for (unsigned i=0; i<num_timelines; ++i)
        {
            const auto& src_line_id = app::ToUtf8(old_properties[QString("timeline_%1_self_id").arg(i)].toString());
            const auto& src_node_id = app::ToUtf8(old_properties[QString("timeline_%1_node_id").arg(i)].toString());
            const auto& dst_line_id = base::RandomString(10);
            const auto& dst_node_id = node_id_map[src_node_id];
            new_properties[QString("timeline_%1_self_id").arg(i)] = app::FromUtf8(dst_line_id);
            new_properties[QString("timeline_%1_node_id").arg(i)] = app::FromUtf8(dst_node_id);
            timeline_id_map[src_line_id] = dst_line_id;
        }
        for (size_t i=0; i<src_track.GetNumActuators(); ++i)
        {
            const auto& src_actuator = src_track.GetActuatorClass(i);
            const auto& dst_actuator = dst_track.GetActuatorClass(i);
            const auto& src_id = src_actuator.GetId();
            const auto& dst_id = dst_actuator.GetId();

            const auto& src_timeline = old_properties[FromUtf8(src_id)].toString();
            const auto& dst_timeline = timeline_id_map[ToUtf8(src_timeline)];
            new_properties[FromUtf8(dst_id)] = FromUtf8(dst_timeline);
        }

        ret["track_" + dst_id] = new_properties;
        ret.remove("track_" + src_id);
    }
    return ret;
}

QStringList ListResourceDependencies(const gfx::PolygonClass& poly, const QVariantMap& props)
{
    // soft dependency
    QStringList ret;
    PushBack(ret, props["material"].toString());
    return ret;

}
QStringList ListResourceDependencies(const gfx::KinematicsParticleEngineClass& particles, const QVariantMap& props)
{
    // soft dependency
    QStringList ret;
    PushBack(ret, props["material"].toString());
    return ret;
}

QStringList ListResourceDependencies(const game::EntityClass& entity, const QVariantMap& props)
{
    QStringList ret;
    PushBack(ret, entity.GetScriptFileId());

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        const auto& node = entity.GetNode(i);
        if (const auto* ptr = node.GetDrawable())
        {
            PushBack(ret, ptr->GetMaterialId());
            PushBack(ret, ptr->GetDrawableId());
        }
        if (const auto* ptr = node.GetRigidBody())
        {
            PushBack(ret, ptr->GetPolygonShapeId());
        }
    }
    return ret;
}

QStringList ListResourceDependencies(const game::SceneClass& scene, const QVariantMap& props)
{
    QStringList ret;
    PushBack(ret, scene.GetScriptFileId());
    PushBack(ret, scene.GetTilemapId());
    for (size_t i=0; i<scene.GetNumNodes(); ++i)
    {
        const auto& node = scene.GetNode(i);
        PushBack(ret, node.GetEntityId());
    }

    return ret;
}
QStringList ListResourceDependencies(const game::TilemapClass& map, const QVariantMap& props)
{
    QStringList ret;

    for (size_t i=0; i<map.GetNumLayers(); ++i)
    {
        const auto& layer = map.GetLayer(i);
        PushBack(ret, layer.GetDataId());

        if (!layer.HasRenderComponent())
            continue;

        for (size_t j=0; j<layer.GetMaxPaletteIndex(); ++j)
        {
            PushBack(ret, layer.GetPaletteMaterialId(j));
        }
    }
    return ret;
}

QStringList ListResourceDependencies(const uik::Window& window, const QVariantMap& props)
{
    QStringList ret;

    PushBack(ret, window.GetScriptFile());

    engine::UIStyle style;
    std::string style_string;
    style_string = window.GetStyleString();
    if (!style_string.empty())
        style.ParseStyleString("window", window.GetStyleString());

    for (size_t i=0; i<window.GetNumWidgets(); ++i)
    {
        const auto& widget = window.GetWidget(i);
        style_string = widget.GetStyleString();
        if (!style_string.empty())
            style.ParseStyleString(widget.GetId(), style_string);
    }

    std::vector<engine::UIStyle::MaterialEntry> materials;
    style.ListMaterials(&materials);
    for (const auto& material : materials)
    {
        if (const auto* ptr = dynamic_cast<const engine::detail::UIMaterialReference*>(material.material))
        {
            PushBack(ret, ptr->GetMaterialId());
        }
    }
    return ret;
}

void PackResource(app::Script& script, ResourcePacker& packer)
{
    const auto& uri = script.GetFileURI();
    packer.CopyFile(uri, "lua/");
    script.SetFileURI(packer.MapUri(uri));
}

void PackResource(app::DataFile& data, ResourcePacker& packer)
{
    const auto& uri = data.GetFileURI();
    packer.CopyFile(uri, "data/");
    data.SetFileURI(packer.MapUri(uri));
}

void PackResource(audio::GraphClass& audio, ResourcePacker& packer)
{
    // todo: this audio packing sucks a little bit since it needs
    // to know about the details of elements now. maybe this
    // should be refactored into the audio/ subsystem.. ?
    for (size_t i=0; i<audio.GetNumElements(); ++i)
    {
        auto& elem = audio.GetElement(i);
        for (auto& p : elem.args)
        {
            const auto& name = p.first;
            if (name != "file") continue;

            auto* file_uri = std::get_if<std::string>(&p.second);
            ASSERT(file_uri && "Missing audio element 'file' parameter.");
            if (file_uri->empty())
            {
                WARN("Audio element doesn't have input file set. [graph='%1', elem='%2']", audio.GetName(), name);
                continue;
            }
            packer.CopyFile(*file_uri, "audio/");
            *file_uri = packer.MapUri(*file_uri);
        }
    }
}

void PackResource(game::EntityClass& entity, ResourcePacker& packer)
{
    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        auto& node = entity.GetNode(i);
        if (!node.HasTextItem())
            continue;
        auto* text = node.GetTextItem();
        const auto& uri = text->GetFontName();
        packer.CopyFile(uri, "fonts/");
        text->SetFontName(packer.MapUri(uri));
    }
}
void PackResource(game::TilemapClass& map, ResourcePacker& packer)
{
    for (size_t i=0; i<map.GetNumLayers(); ++i)
    {
        auto& layer = map.GetLayer(i);
        // todo: maybe fix this implicit assumption here regarding where the
        // data file goes when packing.
        layer.SetDataUri(base::FormatString("pck://data/%1.bin", layer.GetId()));
    }
}

void PackResource(uik::Window& window, ResourcePacker& packer)
{
    engine::UIStyle style;

    // package the style resources. currently, this is only the font files.
    const auto& style_uri  = window.GetStyleName();
    const auto& style_file = packer.ResolveUri(style_uri);

    auto style_data = app::EngineBuffer::LoadFromFile(app::FromUtf8(style_file));
    if (!style_data)
    {
        ERROR("Failed to load UI style file. [UI='%1', style='%2']", window.GetName(), style_file);
    }
    else if (!style.LoadStyle(*style_data))
    {
        ERROR("Failed to parse UI style. [UI='%1', style='%2']", window.GetName(), style_file);
    }
    std::vector<engine::UIStyle::PropertyKeyValue> props;
    style.GatherProperties("-font", &props);
    for (auto& p : props)
    {
        std::string src_font_uri;
        std::string dst_font_uri;
        p.prop.GetValue(&src_font_uri);
        packer.CopyFile(src_font_uri, "fonts");
        dst_font_uri = packer.MapUri(src_font_uri);
        p.prop.SetValue(dst_font_uri);
        style.SetProperty(p.key, p.prop);
    }
    nlohmann::json style_json;
    style.SaveStyle(style_json);
    const auto& style_string_json = style_json.dump(2);
    packer.ReplaceFile(style_uri, "ui", style_string_json.data(), style_string_json.size());

    window.SetStyleName(packer.MapUri(style_uri));

    // for each widget, parse the style string and see if there are more font-name props.
    window.ForEachWidget([&packer](uik::Widget* widget) {
        auto style_string = widget->GetStyleString();
        if (style_string.empty())
            return;
        DEBUG("Original widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);
        engine::UIStyle style;
        style.ParseStyleString(widget->GetId(), style_string);
        std::vector<engine::UIStyle::PropertyKeyValue> props;
        style.GatherProperties("-font", &props);
        for (auto& p : props)
        {
            std::string src_font_uri;
            std::string dst_font_uri;
            p.prop.GetValue(&src_font_uri);
            packer.CopyFile(src_font_uri, "fonts");
            dst_font_uri = packer.MapUri(src_font_uri);
            p.prop.SetValue(dst_font_uri);
            style.SetProperty(p.key, p.prop);
        }
        style_string = style.MakeStyleString(widget->GetId());
        // this is a bit of a hack but we know that the style string
        // contains the widget id for each property. removing the
        // widget id from the style properties:
        // a) saves some space
        // b) makes the style string copyable from one widget to another as-s
        boost::erase_all(style_string, widget->GetId() + "/");
        DEBUG("Updated widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);
        widget->SetStyleString(std::move(style_string));
    });

    // parse the window style string if any and gather/remap font properties.
    auto window_style_string = window.GetStyleString();
    if (!window_style_string.empty())
    {
        DEBUG("Original window style string. [window='%1', style='%2']", window.GetName(), window_style_string);
        engine::UIStyle style;
        style.ParseStyleString("window", window_style_string);
        std::vector<engine::UIStyle::PropertyKeyValue> props;
        style.GatherProperties("-font", &props);
        for (auto& p : props)
        {
            std::string src_font_uri;
            std::string dst_font_uri;
            p.prop.GetValue(&src_font_uri);
            packer.CopyFile(src_font_uri, "fonts");
            dst_font_uri = packer.MapUri(src_font_uri);
            p.prop.SetValue(dst_font_uri);
            style.SetProperty(p.key, p.prop);
        }
        window_style_string = style.MakeStyleString("window");
        // this is a bit of a hack but we know that the style string
        // contains the prefix "window" for each property. removing the
        // prefix from the style properties:
        // a) saves some space
        // b) makes the style string copyable from one widget to another as-s
        boost::erase_all(window_style_string, "window/");
        // set the actual style string.
        DEBUG("Updated window style string. [window='%1', style='%2']", window.GetName(), window_style_string);
        window.SetStyleString(std::move(window_style_string));
    }
}

void PackResource(gfx::MaterialClass& material, ResourcePacker& packer)
{
    if (auto* sprite = material.AsSprite())
    {
        for (size_t i=0; i<sprite->GetNumTextures(); ++i)
        {
            auto* texture_source = sprite->GetTextureSource(i);
            if (auto* text_source = dynamic_cast<gfx::detail::TextureTextBufferSource*>(texture_source))
            {
                auto& text_buffer = text_source->GetTextBuffer();
                for (size_t j=0; j<text_buffer.GetNumTexts(); ++j)
                {
                    auto& text = text_buffer.GetText(j);
                    packer.CopyFile(text.font, "fonts/");
                    text.font = packer.MapUri(text.font);
                }
            }
        }
    }
    else if (auto* texture = material.AsTexture())
    {
        auto* texture_source = texture->GetTextureSource();
        if (auto* text_source = dynamic_cast<gfx::detail::TextureTextBufferSource*>(texture_source))
        {
            auto& text_buffer = text_source->GetTextBuffer();
            for (size_t j=0; j<text_buffer.GetNumTexts(); ++j)
            {
                auto& text = text_buffer.GetText(j);
                packer.CopyFile(text.font, "fonts/");
                text.font = packer.MapUri(text.font);
            }
        }
    }
    else if (auto* custom = material.AsCustom())
    {
        auto texture_maps = custom->GetTextureMapNames();
        for (const auto& name : texture_maps)
        {
            auto* texture_map = custom->FindTextureMap(name);
            if (auto* sprite = texture_map->AsSpriteMap())
            {
                for (size_t i=0; i<sprite->GetNumTextures(); ++i)
                {
                    auto* texture_source = sprite->GetTextureSource(i);
                    if (auto* text_source = dynamic_cast<gfx::detail::TextureTextBufferSource*>(texture_source))
                    {
                        auto& text_buffer = text_source->GetTextBuffer();
                        for (size_t j=0; j<text_buffer.GetNumTexts(); ++j)
                        {
                            auto& text = text_buffer.GetText(j);
                            packer.CopyFile(text.font, "fonts/");
                            text.font  = packer.MapUri(text.font);
                        }
                    }
                }
            }
            else if (auto* texture = texture_map->AsTextureMap2D())
            {
                auto* texture_source = texture->GetTextureSource();
                if (auto* text_source = dynamic_cast<gfx::detail::TextureTextBufferSource*>(texture_source))
                {
                    auto& text_buffer = text_source->GetTextBuffer();
                    for (size_t j=0; j<text_buffer.GetNumTexts(); ++j)
                    {
                        auto& text = text_buffer.GetText(j);
                        packer.CopyFile(text.font, "fonts/");
                        text.font = packer.MapUri(text.font);
                    }
                }
            }
        }
        std::string shader_uri = custom->GetShaderUri();
        if (shader_uri.empty())
            return;
        packer.CopyFile(shader_uri, "shaders/es2");
        shader_uri = packer.MapUri(shader_uri);
        custom->SetShaderUri(shader_uri);
    }
}

} // namespace
} // namespace
