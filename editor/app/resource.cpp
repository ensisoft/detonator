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

#define LOGTAG "app"

#include "config.h"

#include "warnpush.h"
#  include <boost/algorithm/string/erase.hpp>
#  include <nlohmann/json.hpp>
#  include <QRegularExpression>
#  include <QRegularExpressionMatch>
#include "warnpop.h"

#include <unordered_map>
#include <limits>

#include "base/assert.h"
#include "data/chunk.h"
#include "game/timeline_animator.h"
#include "game/timeline_animation.h"
#include "game/entity_state_controller.h"
#include "game/entity_node_rigid_body.h"
#include "game/entity_node_drawable_item.h"
#include "game/entity_node_text_item.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_text_buffer_source.h"
#include "engine/ui.h"
#include "editor/app/buffer.h"
#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/resource.h"
#include "editor/app/resource_packer.h"
#include "editor/app/resource_migration_log.h"

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

    bool PackUIStyleResources(engine::UIStyle& style, app::ResourcePacker& packer)
    {
        bool ok = true;

        std::vector<engine::UIStyle::PropertyKeyValue> props;
        style.GatherProperties("-font", &props);
        for (auto& p : props)
        {
            std::string src_font_uri;
            std::string dst_font_uri;
            p.prop.GetValue(&src_font_uri);
            ok &= packer.CopyFile(src_font_uri, "ui/fonts/");
            dst_font_uri = packer.MapUri(src_font_uri);
            p.prop.SetValue(dst_font_uri);
            style.SetProperty(p.key, p.prop);
        }

        // copy direct UI texture files over into the package.
        // note that since we're copying texture files instead of doing
        // material packing there's no texture packing taking place.
        std::vector<engine::UIStyle::MaterialEntry> materials;
        style.ListMaterials(&materials);
        for (const auto& item : materials)
        {
            auto* material = item.material;
            if (material->GetType() != engine::UIMaterial::Type::Texture)
                continue;
            auto* texture = dynamic_cast<engine::detail::UITexture*>(material);
            std::string src_texture_uri;
            std::string dst_texture_uri;
            src_texture_uri = texture->GetTextureUri();
            ok &= packer.CopyFile(src_texture_uri, "ui/textures/");
            dst_texture_uri = packer.MapUri(src_texture_uri);
            texture->SetTextureUri(dst_texture_uri);
            src_texture_uri = texture->GetMetafileUri();
            if (src_texture_uri.empty())
                continue;
            ok &= packer.CopyFile(src_texture_uri, "ui/textures/");
            dst_texture_uri = packer.MapUri(src_texture_uri);
            texture->SetMetafileUri(dst_texture_uri);
        }
        return ok;
    }

    bool PackUIKeymapFile(uik::Window& window, app::ResourcePacker& packer)
    {
        const auto& keymap_uri = window.GetKeyMapFile();
        if (keymap_uri.empty())
            return true;

        const auto packing_op = packer.GetOp();
        if (packing_op == app::ResourcePacker::Operation::Import)
        {
            if (base::StartsWith(keymap_uri, "app://"))
            {
                DEBUG("Skip importing UI resource that is part of the editor. [UI='%1', uri='%2']",
                      window.GetName(), keymap_uri);
                return true;
            }
        }

        bool ok = true;
        ok &= packer.CopyFile(keymap_uri, "ui/keymap/");
        window.SetKeyMapFile(packer.MapUri(keymap_uri));
        return ok;
    }

    bool PackUIStyleFile(uik::Window& window, app::ResourcePacker& packer)
    {
        const auto& style_uri = window.GetStyleName();

        // if we're importing and the resource is a resource that is part of
        // the editor itself (i.e. starts with app://) then skip importing it
        const auto packing_op = packer.GetOp();
        if  (packing_op == app::ResourcePacker::Operation::Import)
        {
            if (base::StartsWith(style_uri, "app://"))
            {
                DEBUG("Skip importing UI resource that is part of the editor. [UI='%1', uri='%2']",
                      window.GetName(), style_uri);
                return true;
            }
        }
        bool ok = true;

        // package the style resources that are use by the window's
        // main style file. these include raw texture and font URIs.

        QByteArray style_array;
        if (!packer.ReadFile(style_uri, &style_array))
        {
            ERROR("Failed to load UI style file. [UI='%1', style='%2']", window.GetName(), style_uri);
            ok = false;
        }

        engine::UIStyle style;
        if (!style.LoadStyle(app::EngineBuffer("style", std::move(style_array))))
        {
            ERROR("Failed to parse UI style. [UI='%1', style='%2']", window.GetName(), style_uri);
            ok = false;
        }
        ok &= PackUIStyleResources(style, packer);

        nlohmann::json style_json;
        style.SaveStyle(style_json);
        const auto& style_string_json = style_json.dump(2);

        packer.WriteFile(style_uri, "ui/style/", style_string_json.data(), style_string_json.size());
        window.SetStyleName(packer.MapUri(style_uri));
        return ok;
    }


    // Create an anchor using the URI for resolving relative scripts.
    // For example if we have a script URI such as ws://something/lua/foobar.lua
    // which refers to scripts in the same folder the ws://something/lua as
    // the anchor.
    app::AnyString ScriptAnchor(const app::AnyString& uri)
    {
        ASSERT(uri.StartsWith("ws://") ||
               uri.StartsWith("fs://") ||
               uri.StartsWith("app://") ||
               uri.StartsWith("zip://"));

        // for Windows that uses \ for the file path separator.
        QString str = uri.GetWide();
        str.replace("\\", "/");

        QStringList components = str.split("/");
        components.pop_back();
        return components.join("/");
    }

    class LuaBuffer {
    public:
        LuaBuffer(QByteArray& array)
        {
            QByteArray line;
            for (int i=0; i<array.size(); ++i)
            {
                line.append(array[i]);
                if (array[i] == '\n')
                {
                    mLines.push_back(line);
                    line.clear();
                }
            }
            if (!line.isEmpty())
                mLines.push_back(line);
        }
        size_t LineCount() const noexcept
        { return mLines.size(); }
        QString GetLine(size_t i) const noexcept
        { return mLines[i]; }

        QString GetLineOrNothing(size_t i)const noexcept
        {
            if (i >= mLines.size())
                return "";
            return mLines[i];
        }

    private:
        int mPosition = 0;
        std::vector<QString> mLines;
    };

    bool DiscardLuaContent(const LuaBuffer& buffer, size_t* index)
    {
        const auto& line = buffer.GetLine(*index);
        const auto& trim = line.trimmed();
        if (trim.startsWith("--"))
            return true;

        static const char* callbacks[] = {
            "OnBeginContact",
            "OnEndContact",
            "OnKeyDown",
            "OnKeyUp",
            "OnMousePress",
            "OnMouseRelease",
            "OnMouseMove",
            "OnGameEvent",
            "OnAnimationFinished",
            "OnEntityTimer",
            "OnEntityEvent",
            "OnTimer",
            "OnEvent",
            "OnUIOpen",
            "OnUIClose",
            "OnUIAction",
            "OnAudioEvent",
            "Tick",
            "Update",
            "UpdateNodes",
            "PostUpdate",
            "BeginPlay",
            "EndPlay",
            "SpawnEntity",
            "KillEntity"
        };

        for (size_t i=0; i<base::ArraySize(callbacks); ++i)
        {
            if (trim.startsWith("function") && trim.contains(callbacks[i])) {
                if (buffer.GetLineOrNothing(*index + 1).startsWith("end")) {
                    *index += 2;
                    return true;
                }
            }
        }
        return false;
    }

    // pack script with dependencies by recursively reading
    // the script files and looking for other scripts via require
    bool PackScriptRecursive(const app::AnyString& uri,
                             const app::AnyString& dir,
                             const app::AnyString& anchor,
                             app::ResourcePacker& packer)
    {
        // Read the contents of the Lua script file and look for dependent scripts
        // somehow we need to resolve those dependent scripts in order to package
        // everything properly.
        QByteArray src_buffer;
        if (!packer.ReadFile(uri, &src_buffer))
        {
            ERROR("Failed to read script file. [uri='%1']", uri);
            return false;
        }

        QByteArray dst_buffer;
        QTextStream dst_stream(&dst_buffer);
        dst_stream.setCodec("UTF-8");
        LuaBuffer src_stream(src_buffer);

        bool ok = true;

        for (size_t i=0; i<src_stream.LineCount(); ++i)
        {
            QString line = src_stream.GetLine(i);
            QRegularExpression require;
            // \s* matches any number of empty space
            // [^']* matches any number of any characters except for '
            // (foo) is a capture group in Qt regex
            require.setPattern(R"(\s*require\('([^']*)'\)\s*)");
            ASSERT(require.isValid());
            QRegularExpressionMatch match = require.match(line);

            if (!match.hasMatch())
            {
                if (!packer.IsReleasePackage() || !DiscardLuaContent(src_stream, &i))
                    dst_stream << line;
                continue;
            }

            ASSERT(require.captureCount() == 1);
            // cap(0) is the whole regex
            QString module = match.captured(1);
            if (module.startsWith("app://") ||
                module.startsWith("fs://")  ||
                module.startsWith("ws://"))
            {
                DEBUG("Found dependent script. '%1' depends on '%2'.", uri, module);
                if (!module.endsWith(".lua"))
                    module.append(".lua");

                // big recursive call here. maybe this should be postponed
                // and we should just explore the requires first and then
                // visit the files instead of keeping all the current state
                // and then recursing.?
                ok &= PackScriptRecursive(module, "lua/",  ScriptAnchor(module), packer);
                line.replace(module, packer.MapUri(module));
            }
            else
            {
                // relative path for example we have a scripts
                // lua/game_script.lua
                // lua/foo/foo.lua
                //
                // and game_script.lua does
                //
                //     require('foo/foo.lua')
                //
                //
                QStringList paths = module.split("/");
                paths.push_front("lua");
                paths.pop_back();
                QString path = paths.join("/");

                DEBUG("Found dependent script. '%1' depends on '%2'.", uri, module);
                if (!module.endsWith(".lua"))
                    module.append(".lua");

                module = anchor + "/" + module;

                DEBUG("Dependent script generated path is '%1'", module);
                ok &= PackScriptRecursive(module, path, anchor, packer);

                // do not change the outgoing line here, so it remains the
                // same, for example require('foo/foo.lua')
            }
            dst_stream << line;
        }
        dst_stream.flush();

        ok &= packer.WriteFile(uri, dir, dst_buffer.constData(), dst_buffer.length());
        if (!ok) {
            ERROR("Error while packing script. [uri='%1']", uri);
        }
        return ok;
    }

} // namespace

namespace app {
namespace detail {
QVariantMap DuplicateResourceProperties(const game::EntityClass& src, game::EntityClass& dupe, const QVariantMap& props)
{
    ASSERT(src.GetNumNodes() == dupe.GetNumNodes());
    ASSERT(src.GetNumAnimations() == dupe.GetNumAnimations());
    ASSERT(src.GetNumAnimators() == dupe.GetNumAnimators());

    QVariantMap ret = props;

    // map the properties associated with the resource object, i.e. the variant map
    // from properties using the old IDs to properties using new IDs
    for (size_t i=0; i<src.GetNumAnimators(); ++i)
    {
        const auto& src_animator = src.GetController(i);
        const auto& dst_animator = dupe.GetController(i);
        // map state and link IDs from the src animator IDs to the
        // duplicate animator IDs
        std::unordered_map<std::string, std::string> mapping;
        for (size_t i=0; i<src_animator.GetNumStates(); ++i)
        {
            const auto& src_state = src_animator.GetState(i);
            const auto& dst_state = dst_animator.GetState(i);
            mapping[src_state.GetId()] = dst_state.GetId();
        }
        for (const auto& [old_id, new_id] : mapping)
        {
            const float xpos = props[app::PropertyKey("scene_pos_x", old_id)].toFloat();
            const float ypos = props[app::PropertyKey("scene_pos_y", old_id)].toFloat();
            ret.remove(app::PropertyKey("scene_pos_x", old_id));
            ret.remove(app::PropertyKey("scene_pos_y", old_id));
            ret[app::PropertyKey("scene_pos_x", new_id)] = xpos;
            ret[app::PropertyKey("scene_pos_y", new_id)] = ypos;
        }

        mapping.clear();

        for (size_t i=0; i<src_animator.GetNumTransitions(); ++i)
        {
            const auto& src_transition = src_animator.GetTransition(i);
            const auto& dst_transition = dst_animator.GetTransition(i);
            mapping[src_transition.GetId()] = dst_transition.GetId();
        }

        for (const auto& [old_id, new_id] : mapping)
        {
            const float srcx = props[app::PropertyKey("src_point_x", old_id)].toFloat();
            const float srcy = props[app::PropertyKey("src_point_y", old_id)].toFloat();
            const float dstx = props[app::PropertyKey("dst_point_x", old_id)].toFloat();
            const float dsty = props[app::PropertyKey("dst_point_y", old_id)].toFloat();
            const float posx = props[app::PropertyKey("scene_pos_x", old_id)].toFloat();
            const float posy = props[app::PropertyKey("scene_pos_y", old_id)].toFloat();

            ret.remove(app::PropertyKey("src_point_x", old_id));
            ret.remove(app::PropertyKey("src_point_y", old_id));
            ret.remove(app::PropertyKey("dst_point_x", old_id));
            ret.remove(app::PropertyKey("dst_point_y", old_id));
            ret.remove(app::PropertyKey("scene_pos_x", old_id));
            ret.remove(app::PropertyKey("scene_pos_y", old_id));

            ret[app::PropertyKey("src_point_x", old_id)] = srcx;
            ret[app::PropertyKey("src_point_y", old_id)] = srcy;
            ret[app::PropertyKey("dst_point_x", old_id)] = dstx;
            ret[app::PropertyKey("dst_point_y", old_id)] = dsty;
            ret[app::PropertyKey("scene_pos_x", old_id)] = posx;
            ret[app::PropertyKey("scene_pos_y", old_id)] = posy;
        }
    }

    std::unordered_map<AnyString, AnyString> node_id_map;
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
        auto& dst_track = dupe.GetAnimation(i);
        const auto& src_id    = FromUtf8(src_track.GetId());
        const auto& dst_id    = FromUtf8(dst_track.GetId());
        const auto& variant   = ret.value("track_" + src_id);
        if (variant.isNull())
            continue;

        ASSERT(src_track.GetNumAnimators() == dst_track.GetNumAnimators());

        std::unordered_map<AnyString, AnyString> timeline_id_map;

        QVariantMap new_properties;
        QVariantMap old_properties = variant.toMap();
        const auto num_timelines = old_properties["num_timelines"].toUInt();
        for (unsigned i=0; i<num_timelines; ++i)
        {
            const auto& src_timeline_id    = old_properties[QString("timeline_%1_self_id").arg(i)].toString();
            const auto& src_target_node_id = old_properties[QString("timeline_%1_node_id").arg(i)].toString();
            const auto& dst_timeline_id    = AnyString(base::RandomString(10));
            const auto& dst_target_node_id = node_id_map[src_target_node_id];
            new_properties[QString("timeline_%1_self_id").arg(i)] = dst_timeline_id;
            new_properties[QString("timeline_%1_node_id").arg(i)] = dst_target_node_id;
            timeline_id_map[src_timeline_id] = dst_timeline_id;
        }
        for (size_t i=0; i< src_track.GetNumAnimators(); ++i)
        {
            const auto& src_animator = src_track.GetAnimatorClass(i);
            auto& dst_animator = dst_track.GetAnimatorClass(i);
            const auto& src_id = AnyString(src_animator.GetId());
            const auto& dst_id = AnyString(dst_animator.GetId());

            if (old_properties.contains(src_id))
            {
                const auto& src_timeline = old_properties[src_id].toString();
                const auto& dst_timeline = timeline_id_map[src_timeline];
                new_properties[dst_id] = dst_timeline;
            }
            const auto& src_timeline_id = src_animator.GetTimelineId();
            if (!src_timeline_id.empty())
                dst_animator.SetTimelineId(timeline_id_map[src_timeline_id]);
        }

        ret["track_" + dst_id] = new_properties;
        ret.remove("track_" + src_id);
    }
    return ret;
}

QStringList ListResourceDependencies(const gfx::PolygonMeshClass& poly, const QVariantMap& props)
{
    // soft dependency
    QStringList ret;
    PushBack(ret, props["material"].toString());
    return ret;

}
QStringList ListResourceDependencies(const gfx::ParticleEngineClass& particles, const QVariantMap& props)
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

    for (size_t i=0; i<entity.GetNumAnimators(); ++i)
    {
        const auto& animator = entity.GetController(0);
        if (animator.HasScriptId())
            PushBack(ret, animator.GetScriptId());
    }

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
        const auto& placement = scene.GetPlacement(i);
        PushBack(ret, placement.GetEntityId());
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

bool PackResource(app::Script& script, ResourcePacker& packer)
{
    const auto& uri = script.GetFileURI();

    bool ok = true;

    ok &= PackScriptRecursive(uri, "lua/", ScriptAnchor(uri), packer);

    script.SetFileURI(packer.MapUri(uri));

    return ok;
}

bool PackResource(app::DataFile& data, ResourcePacker& packer)
{
    bool ok = true;

    const auto& uri = data.GetFileURI();
    ok &= packer.CopyFile(uri, "data/");
    data.SetFileURI(packer.MapUri(uri));

    return ok;
}

bool PackResource(audio::GraphClass& audio, ResourcePacker& packer)
{
    bool ok = true;

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
            ok &= packer.CopyFile(*file_uri, "audio/");
            *file_uri = packer.MapUri(*file_uri);
        }
    }
    return ok;
}

bool PackResource(game::EntityClass& entity, ResourcePacker& packer)
{
    bool ok = true;

    for (size_t i=0; i<entity.GetNumNodes(); ++i)
    {
        auto& node = entity.GetNode(i);
        if (!node.HasTextItem())
            continue;
        auto* text = node.GetTextItem();
        const auto& uri = text->GetFontName();
        ok &= packer.CopyFile(uri, "fonts/");
        text->SetFontName(packer.MapUri(uri));
    }

    return ok;
}
bool PackResource(game::TilemapClass& map, ResourcePacker& packer)
{
    bool ok = true;

    // there's an important requirement regarding resource packing order.
    // The tilemap layer refers to a data object for the level data,
    // and when doing URI mapping this means that the packager must have
    // already seen the data object, or otherwise the mapping cannot work.
    // this means the data object must be packed before the tilemap!
    for (size_t i=0; i<map.GetNumLayers(); ++i)
    {
        auto& layer = map.GetLayer(i);
        const auto& uri = layer.GetDataUri();
        layer.SetDataUri(packer.MapUri(uri));
    }
    return ok;
}

bool PackResource(uik::Window& window, ResourcePacker& packer)
{
    bool ok = true;
    ok &= PackUIKeymapFile(window, packer);
    ok &= PackUIStyleFile(window, packer);

    // for each widget, parse the style string and see if there are more font-name props.
    window.ForEachWidget([&packer, &ok](uik::Widget* widget) {
        auto style_string = widget->GetStyleString();
        if (style_string.empty())
            return;

        if (packer.IsReleasePackage())
            VERBOSE("Original widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);

        engine::UIStyle style;
        style.ParseStyleString(widget->GetId(), style_string);

        ok &= PackUIStyleResources(style, packer);

        if (packer.NeedsRemapping())
        {
            style_string = style.MakeStyleString(widget->GetId());
            // this is a bit of a hack but we know that the style string
            // contains the widget id for each property. removing the
            // widget id from the style properties:
            // a) saves some space
            // b) makes the style string copyable from one widget to another as-s
            boost::erase_all(style_string, widget->GetId() + "/");
            widget->SetStyleString(style_string);
        }

        if (packer.IsReleasePackage())
            VERBOSE("Updated widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);
    });

    // parse the window style string if any and gather/remap font properties.
    auto window_style_string = window.GetStyleString();
    if (!window_style_string.empty())
    {
        if (packer.IsReleasePackage())
            VERBOSE("Original window style string. [window='%1', style='%2']", window.GetName(), window_style_string);

        engine::UIStyle style;
        style.ParseStyleString("window", window_style_string);

        ok &= PackUIStyleResources(style, packer);

        if (packer.NeedsRemapping())
        {
            window_style_string = style.MakeStyleString("window");
            // this is a bit of a hack, but we know that the style string
            // contains the prefix "window" for each property. removing the
            // prefix from the style properties:
            // a) saves some space
            // b) makes the style string copyable from one widget to another as-s
            boost::erase_all(window_style_string, "window/");
            // set the actual style string.
            window.SetStyleString(window_style_string);
        }

        if (packer.IsReleasePackage())
            VERBOSE("Updated window style string. [window='%1', style='%2']", window.GetName(), window_style_string);

    }
    return ok;
}

bool PackResource(gfx::MaterialClass& material, ResourcePacker& packer)
{
    bool ok = true;

    for (size_t i=0; i<material.GetNumTextureMaps(); ++i)
    {
        auto* map = material.GetTextureMap(i);
        for (size_t j=0; j<map->GetNumTextures(); ++j)
        {
            auto* src = map->GetTextureSource(j);
            if (auto* text_source = dynamic_cast<gfx::TextureTextBufferSource*>(src))
            {
                auto& text_buffer = text_source->GetTextBuffer();
                auto& text_chunk  = text_buffer.GetText();
                ok &= packer.CopyFile(text_chunk.font, "fonts/");
                text_chunk.font = packer.MapUri(text_chunk.font);
            }
            else if (auto* file_source = dynamic_cast<gfx::TextureFileSource*>(src))
            {
                const auto& uri = file_source->GetFilename();
                ok &= packer.CopyFile(uri, "textures/");
                file_source->SetFileName(packer.MapUri(uri));
            }
        }
    }

    const auto& shader_glsl_uri = material.GetShaderUri();
    if (shader_glsl_uri.empty())
        return ok;

    const auto& shader_desc_uri = boost::replace_all_copy(shader_glsl_uri, ".glsl", ".json");
    // this only has significance when exporting/importing
    // a resource archive.
    ok &= packer.CopyFile(shader_desc_uri, "shaders/es2/");
    // copy the actual shader glsl
    ok &= packer.CopyFile(shader_glsl_uri, "shaders/es2/");
    material.SetShaderUri(packer.MapUri(shader_glsl_uri));
    return ok;
}

template<>
std::unique_ptr<data::Chunk> MigrateResourceDataChunk<game::EntityClass>(std::unique_ptr<data::Chunk> chunk, ResourceMigrationLog* log)
{
    std::string resource_name;
    std::string resource_id;
    chunk->GetReader()->Read("resource_name", &resource_name);
    chunk->GetReader()->Read("resource_id", &resource_id);

    for (unsigned i=0; i<chunk->GetReader()->GetNumChunks("tracks"); ++i)
    {
        auto animation_chunk = chunk->GetReader()->GetChunk("tracks", i);
        if (!animation_chunk)
            continue;

        for (unsigned i=0; i<animation_chunk->GetReader()->GetNumChunks("actuators"); ++i)
        {
            auto actuator_meta_chunk = animation_chunk->GetReader()->GetChunk("actuators", i);
            auto actuator_data_chunk = actuator_meta_chunk->GetReader()->GetChunk("actuator");
            if (!actuator_data_chunk || !actuator_meta_chunk)
                continue;

            std::string old_type_string;
            std::string new_type_string;
            actuator_meta_chunk->GetReader()->Read("type", &old_type_string);

            // map old type string to new type string
            if (old_type_string == "Transform")
                new_type_string = "TransformAnimator";
            else if (old_type_string == "Material")
                new_type_string = "MaterialAnimator";
            else if (old_type_string == "Kinematic")
                new_type_string = "KinematicAnimator";
            else if (old_type_string == "SetFlag")
                new_type_string = "BooleanPropertyAnimator";
            else if (old_type_string == "SetValue")
                new_type_string = "PropertyAnimator";
            else new_type_string = old_type_string;

            actuator_meta_chunk->GetWriter()->Write("type", new_type_string);
            if (new_type_string != old_type_string)
            {
                if (log)
                    log->WriteLog(resource_id, resource_name, "EntityClass", toString("Animator type mapped from '%1' to '%2'", old_type_string, new_type_string));
            }
            //actuator_meta_chunk->OverwriteChunk("actuator", std::move(actuator_data_chunk));
            //animation_chunk->OverwriteChunk("actuators", std::move(actuator_meta_chunk), i);
            actuator_meta_chunk->GetWriter()->Write("animator", std::move(actuator_data_chunk));
            animation_chunk->GetWriter()->AppendChunk("animators", std::move(actuator_meta_chunk));
        }
        chunk->OverwriteChunk("tracks", std::move(animation_chunk), i);
    }
    return chunk;
}

void MigrateResource(uik::Window& window, app::ResourceMigrationLog* log, unsigned old_version, unsigned  new_version)
{
    // migration path for old data which doesn't yet have tab order values.
    const auto& keymap_uri = window.GetKeyMapFile();
    if (keymap_uri.empty())
    {
        window.SetKeyMapFile("app://ui/keymap/default.json");
        if (log)
        {
            log->WriteLog(window, "UI", "Added default keymap file.");
        }
    }

    unsigned focusable_widget_count = 0;

    // if all widgets have 0 as tab index then initialize here.
    bool does_have_tab_ordering = false;
    bool does_need_tab_ordering = false;

    for (size_t i=0; i<window.GetNumWidgets(); ++i)
    {
        const auto& widget = window.GetWidget(i);
        if (!widget.CanFocus())
            continue;
        focusable_widget_count++;
        does_need_tab_ordering = true;
        if (widget.GetTabIndex() != 0u)
        {
            does_have_tab_ordering = true;
            break;
        }
    }
    if (focusable_widget_count == 0 || focusable_widget_count == 1)
        return;

    if (!does_need_tab_ordering || does_have_tab_ordering)
        return;

    if (log)
    {
        log->WriteLog(window, "UI", "Generated UI widget tab order.");
    }

    // generate tab order values if none yet exist.
    unsigned tab_index = 0;
    for (size_t i=0; i<window.GetNumWidgets(); ++i)
    {
        auto& widget = window.GetWidget(i);
        if (!widget.CanFocus())
            continue;
        widget.SetTabIndex(tab_index++);
    }
}

void MigrateResource(gfx::MaterialClass& material, ResourceMigrationLog* log, unsigned old_version, unsigned new_version)
{
    DEBUG("Migrating material resource. [material='%1']", material.GetName());

    if (old_version == 0)
    {
        for (unsigned i = 0; i < material.GetNumTextureMaps(); ++i)
        {
            auto* map = material.GetTextureMap(i);
            for (unsigned j = 0; j < map->GetNumTextures(); ++j)
            {
                auto* source = map->GetTextureSource(j);
                if (auto* ptr = dynamic_cast<gfx::TextureFileSource*>(source))
                {
                    ptr->SetColorSpace(gfx::TextureFileSource::ColorSpace::sRGB);
                    DEBUG("Changing material texture color space to sRGB. [material='%1', texture='%2']", material.GetName(), source->GetName());
                    log->WriteLog(material, "Material", "Changed texture color space to sRGB from linear.");
                }
            }
        }
    }

    // we're squashing the particle shaders into one in order
    // to simplify the shader maintenance
    if (old_version < 2)
    {
        std::string shader_uri = material.GetShaderUri();
        if (shader_uri == "app://shaders/es2/emissive_particle.glsl")
        {
            material.SetShaderUri("app://shaders/es2/basic_particle.glsl");
            log->WriteLog(material, "Material", "Changed emissive particle to basic particle that does the same thing.");
        }
    }
    if (old_version < 3)
    {
        const auto& shader_uri = material.GetShaderUri();
        if (shader_uri == "app://shaders/es2/basic_particle.glsl")
        {
            if (material.HasUniform("kRotate") && material.CheckUniformType<float>("kRotate"))
            {
                const auto value = material.GetUniformValue<float>("kRotate", 0.0);
                material.DeleteUniform("kRotate");
                if (value == 0.0f)
                    material.SetUniform("kRotate", 0);
                else if (value == 1.0f)
                    material.SetUniform("kRotate", 1); // random rotation

                log->WriteLog(material, "Material", "Changed kRotate uniform from float to int.");
            }
            if (material.HasUniform("kRotationalVelocity") && material.CheckUniformType<float>("kRotationalVelocity"))
            {
                const auto value = material.GetUniformValue<float>("kRotationalVelocity", 0.0f);
                material.DeleteUniform("kRotationalVelocity");
                material.SetUniform("kRotationValue", value);
                log->WriteLog(material, "Material", "Changed kRotationVelocity uniform to kRotationValue uniform");
            }
        }
    }
    if (old_version < 4)
    {
        const auto& shader_uri = material.GetShaderUri();
        if (shader_uri == "app://shaders/es2/basic_particle.glsl")
        {
            material.SetType(gfx::MaterialClass::Type::Particle2D);
            const auto kStartColor = material.GetUniformValue("kStartColor", gfx::Color4f(gfx::Color::White));
            const auto kEndColor = material.GetUniformValue("kEndColor", gfx::Color4f(gfx::Color::White));
            const auto kRotationValue = material.GetUniformValue("kRotation", 0.0f);
            const auto kRotate = material.GetUniformValue("kRotate", 0);

            material.SetParticleStartColor(kStartColor);
            material.SetParticleEndColor(kEndColor);
            material.SetParticleBaseRotation(kRotationValue);
            if (kRotate > 0) // 0 = OFF = maps to the same value.
                material.SetParticleRotation(static_cast<gfx::MaterialClass::ParticleRotation>(kRotate+1));

            log->WriteLog(material, "Material", "Migrated to built-in Particle2D material and shader.");
            if (material.GetNumTextureMaps())
            {
                auto* map = material.GetTextureMap(0);
                map->SetSamplerName("kMask", 0);
                map->SetRectUniformName("kMaskRect", 0);
                map->SetName("Particle Alpha Mask");
            }
            material.DeleteUniform("kStartColor");
            material.DeleteUniform("kEndColor");
            material.DeleteUniform("kRotationValue");
            material.DeleteUniform("kRotate");
            material.SetShaderUri("");
        }
    }

    if (old_version < 6)
    {
        if (material.GetType() == gfx::MaterialClass::Type::Particle2D)
        {
            if (!material.HasUniform("kParticleMidColor"))
            {
                const auto& start_color = material.GetParticleStartColor();
                const auto& end_color = material.GetParticleEndColor();
                const auto& mid_color = start_color*0.5f + end_color *0.5f;
                material.SetParticleMidColor(mid_color);
                log->WriteLog(material, "Material", "Added new particle mid-way color value.");
            }
        }
    }

    // the uniform values were refactored inside the material class
    // and they only exist now if they have been set explicitly.
    // we can clean away the uniforms that have *not* been set by the
    // user, i.e. have the same value as the default that takes place
    // when the value isn't set.
    using ColorIndex = gfx::MaterialClass::ColorIndex;

    if (const auto* ptr = material.FindUniformValue<gfx::Color4f>("kBaseColor"))
    {
        if (Equals(*ptr, gfx::Color::White))
        {
            material.DeleteUniform("kBaseColor");
            log->WriteLog(material, "Material", "Removed unused default value on 'base color'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<gfx::Color4f>("kColor0"))
    {
        if (Equals(*ptr, gfx::Color::White))
        {
            material.DeleteUniform("kColor0");
            log->WriteLog(material, "Material", "Removed unused default value on 'top left gradient color'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<gfx::Color4f>("kColor1"))
    {
        if (Equals(*ptr, gfx::Color::White))
        {
            material.DeleteUniform("kColor1");
            log->WriteLog(material, "Material", "Removed unused default value on 'top right gradient color'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<gfx::Color4f>("kColor2"))
    {
        if (Equals(*ptr, gfx::Color::White))
        {
            material.DeleteUniform("kColor2");
            log->WriteLog(material, "Material", "Removed unused default value on 'bottom left gradient color'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<gfx::Color4f>("kColor3"))
    {
        if (Equals(*ptr, gfx::Color::White))
        {
            material.DeleteUniform("kColor3");
            log->WriteLog(material, "Material", "Removed unused default value on 'bottom right gradient color'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<glm::vec3>("kTextureVelocity"))
    {
        if (math::equals(*ptr, glm::vec3(0.0f, 0.0f, 0.0f)))
        {
            material.DeleteUniform("kTextureVelocity");
            log->WriteLog(material, "Material", "Removed unused default value on 'texture velocity'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<glm::vec2>("kTextureScale"))
    {
        if (math::equals(*ptr, glm::vec2(1.0f, 1.0f)))
        {
            material.DeleteUniform("kTextureScale");
            log->WriteLog(material, "Material", "Removed unused default value on 'texture scale'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<float>("KTextureRotation"))
    {
        if (math::equals(*ptr, 0.0f))
        {
            material.DeleteUniform("kTextureRotation");
            log->WriteLog(material, "Material", "Removed unused default value on 'texture rotation'.");
        }
    }

    if (const auto* ptr = material.FindUniformValue<glm::vec2>("kWeight"))
    {
        if (math::equals(*ptr, glm::vec2(0.5f, 0.5f)))
        {
            material.DeleteUniform("kWeight");
            log->WriteLog(material, "Material", "Removed unused default value on 'gradient mix weight'.");
        }
    }

    if (material.GetType() == gfx::MaterialClass::Type::Gradient)
    {
        if (const auto* ptr = material.FindUniformValue<glm::vec2>("kWeight"))
        {
            material.SetUniform("kGradientWeight", *ptr);
            material.DeleteUniform("kWeight");
            log->WriteLog(material, "Material", "Changed gradient uniform name from kWeight to kGradientWeight");
        }

        for (int i=0; i<4; ++i)
        {
            const auto& old_name = base::FormatString("kColor%1", i);
            const auto& new_name = base::FormatString("kGradientColor%1", i);
            if (const auto* ptr = material.FindUniformValue<gfx::Color4f>(old_name))
            {
                material.SetUniform(new_name, *ptr);
                material.DeleteUniform(old_name);
                log->WriteLog(material, "Material", base::FormatString("Changed gradient uniform name from %1 to %2",
                    old_name, new_name));
            }
        }
    }

    if (material.GetType() != gfx::MaterialClass::Type::Custom)
    {
        if (material.HasShaderUri())
        {
            log->WriteLog(material, "Material", "Built-in material uses a custom shader source. This is no longer supported.");
            material.ClearShaderUri();
        }
    }

}

void MigrateResource(gfx::ParticleEngineClass& particles, ResourceMigrationLog* log, unsigned old_version, unsigned  new_version)
{
    auto& params = particles.GetParams();

    if (old_version < 2)
    {
        if (params.min_lifetime == std::numeric_limits<float>::max() &&
            params.max_lifetime == std::numeric_limits<float>::max())
        {
            params.flags.set(gfx::ParticleEngineClass::Flags::ParticlesCanExpire, false);
            log->WriteLog(particles, "Particles", "Enabled particle expiration flag.");
        }
    }
}

void MigrateResource(game::EntityClass& entity, ResourceMigrationLog* log, unsigned old_version, unsigned new_version)
{
    if (old_version < 2)
    {
        bool did_migrate = false;
        for (unsigned i=0; i<entity.GetNumNodes(); ++i)
        {
            auto& node = entity.GetNode(i);
            if (!node.HasDrawable())
                continue;

            auto* drawable = node.GetDrawable();
            if (drawable->HasMaterialParam("kEndColor"))
            {
                auto value = drawable->GetMaterialParamValue<game::Color4f>("kEndColor");
                drawable->SetMaterialParam("kParticleEndColor", *value);
                drawable->DeleteMaterialParam("kEndColor");
                did_migrate = true;
            }
            if (drawable->HasMaterialParam("kStartColor"))
            {
                auto value = drawable->GetMaterialParamValue<game::Color4f>("kStartColor");
                drawable->SetMaterialParam("kParticleStartColor", *value);
                drawable->DeleteMaterialParam("kStartColor");
                did_migrate = true;
            }
        }
        if (did_migrate)
        {
            log->WriteLog(entity, "Entity", "Migrated to built-in particle 2D material entity node uniforms.");
        }
    }
    if (old_version < 3)
    {
        for (unsigned i=0; i<entity.GetNumNodes(); ++i)
        {
            auto& node = entity.GetNode(i);
            if (!node.HasDrawable())
                continue;

            auto* drawable = node.GetDrawable();
            if (drawable->HasMaterialParam("kParticleStartColor") &&
                drawable->HasMaterialParam("kParticleEndColor") &&
                !drawable->HasMaterialParam("kParticleMidColor"))
            {
                const auto* start_color = drawable->GetMaterialParamValue<game::Color4f>("kParticleStartColor");
                const auto* end_color = drawable->GetMaterialParamValue<game::Color4f>("kParticleEndColor");
                if (start_color && end_color)
                {
                    const auto& mid_color = *start_color*0.5f + *end_color*0.5f;
                    drawable->SetMaterialParam("kParticleMidColor", mid_color);
                    log->WriteLog(entity, "Entity", "Fabricated entity drawable particle mid color value.");
                }
            }
        }
    }
}

} // namespace

QVariant ResourceListModel::data(const QModelIndex& index, int role) const
{
    ASSERT(index.model() == this);
    ASSERT(index.row() < mResources.size());
    const auto& item = mResources[index.row()];

    if (role == Qt::SizeHintRole)
        return QSize(0, 16);
    else if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case 0: return toString(item.resource->GetType());
            case 1: return toString(item.resource->GetName());
        }
    }
    else if (role == Qt::DecorationRole && index.column() == 0)
    {
        return item.resource->GetIcon();
    }
    return QVariant();
}
QVariant ResourceListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case 0:  return "Type";
            case 1:  return "Name";
        }
    }
    return QVariant();
}

void ResourceListModel::SetList(const ResourceList& list)
{
    QAbstractTableModel::beginResetModel();
    mResources = list;
    QAbstractTableModel::endResetModel();
}

void ResourceListModel::Clear()
{
    QAbstractTableModel::beginResetModel();
    mResources.clear();
    QAbstractTableModel::endResetModel();
}

} // namespace
