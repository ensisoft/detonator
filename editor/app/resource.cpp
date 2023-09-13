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

    void PackUIStyleResources(engine::UIStyle& style, app::ResourcePacker& packer)
    {
        std::vector<engine::UIStyle::PropertyKeyValue> props;
        style.GatherProperties("-font", &props);
        for (auto& p : props)
        {
            std::string src_font_uri;
            std::string dst_font_uri;
            p.prop.GetValue(&src_font_uri);
            packer.CopyFile(src_font_uri, "fonts/");
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
            packer.CopyFile(src_texture_uri, "/ui/textures/");
            dst_texture_uri = packer.MapUri(src_texture_uri);
            texture->SetTextureUri(dst_texture_uri);
            src_texture_uri = texture->GetMetafileUri();
            if (src_texture_uri.empty())
                continue;
            packer.CopyFile(src_texture_uri, "/ui/textures/");
            dst_texture_uri = packer.MapUri(src_texture_uri);
            texture->SetMetafileUri(dst_texture_uri);
        }
    }

    // pack script with dependencies by recursively reading
    // the script files and looking for other scripts via require
    void PackScriptRecursive(const app::AnyString& uri, app::ResourcePacker& packer)
    {
        // Read the contents of the Lua script file and look for dependent scripts
        // somehow we need to resolve those dependent scripts in order to package
        // everything properly.
        QByteArray src_buffer;
        if (!packer.ReadFile(uri, &src_buffer))
        {
            ERROR("Failed to read script file. [uri='%1']", uri);
            return;
        }

        QByteArray dst_buffer;
        QTextStream dst_stream(&dst_buffer);
        dst_stream.setCodec("UTF-8");

        class MyStream {
        public:
            MyStream(QByteArray& array) : mArray(array)
            {}
            QString ReadLine()
            {
                QByteArray tmp;
                for (;mPosition<mArray.size(); ++mPosition)
                {
                    tmp.append(mArray[mPosition]);
                    if (mArray[mPosition] == '\n')
                    {
                        ++mPosition;
                        break;
                    }
                }
                return QString::fromUtf8(tmp);
            }
            bool AtEnd() const
            { return mPosition == mArray.size(); }
        private:
            const QByteArray& mArray;
            int mPosition = 0;
        } src_stream(src_buffer);

        while (!src_stream.AtEnd())
        {
            QString line = src_stream.ReadLine();
            QRegularExpression require;
            // \s* matches any number of empty space
            // [^']* matches any number of any characters except for '
            // (foo) is a capture group in Qt regex
            require.setPattern(R"(\s*require\('([^']*)'\)\s*)");
            ASSERT(require.isValid());
            QRegularExpressionMatch match = require.match(line);
            if (!match.hasMatch())
            {
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
                DEBUG("Found dependent script. %1 depends on %2", uri, module);
                if (!module.endsWith(".lua"))
                    module.append(".lua");

                // big recursive call here. maybe this should be postponed
                // and we should just explore the requires first and then
                // visit the files instead of keeping all the current state
                // and then recursing.?
                PackScriptRecursive(module, packer);

                packer.CopyFile(module, "lua/");
                line.replace(module, packer.MapUri(module));
            }
            dst_stream << line;
        }
        dst_stream.flush();

        packer.WriteFile(uri, "lua/", dst_buffer.constData(), dst_buffer.length());
    }

} // namespace

namespace app {
namespace detail {
QVariantMap DuplicateResourceProperties(const game::EntityClass& src, const game::EntityClass& dupe, const QVariantMap& props)
{
    ASSERT(src.GetNumNodes() == dupe.GetNumNodes());
    ASSERT(src.GetNumAnimations() == dupe.GetNumAnimations());
    ASSERT(src.GetNumAnimators() == dupe.GetNumAnimators());

    QVariantMap ret = props;

    // map the properties associated with the resource object, i.e. the variant map
    // from properties using the old IDs to properties using new IDs
    for (size_t i=0; i<src.GetNumAnimators(); ++i)
    {
        const auto& src_animator = src.GetAnimator(i);
        const auto& dst_animator = dupe.GetAnimator(i);
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

    for (size_t i=0; i<entity.GetNumAnimators(); ++i)
    {
        const auto& animator = entity.GetAnimator(0);
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

void PackResource(app::Script& script, ResourcePacker& packer)
{
    const auto& uri = script.GetFileURI();

    PackScriptRecursive(uri, packer);
    //packer.CopyFile(uri, "lua/");

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
}

void PackResource(uik::Window& window, ResourcePacker& packer)
{
    engine::UIStyle style;

    const auto& keymap_uri = window.GetKeyMapFile();
    if (!keymap_uri.empty())
    {
        packer.CopyFile(keymap_uri, "ui/keymap/");
        window.SetKeyMapFile(packer.MapUri(keymap_uri));
    }

    // package the style resources. currently, this is only the font files.
    const auto& style_uri  = window.GetStyleName();

    QByteArray style_array;
    if (!packer.ReadFile(style_uri, &style_array))
    {
        ERROR("Failed to load UI style file. [UI='%1', style='%2']", window.GetName(), style_uri);
    }
    if (!style.LoadStyle(app::EngineBuffer("style", std::move(style_array))))
    {
        ERROR("Failed to parse UI style. [UI='%1', style='%2']", window.GetName(), style_uri);
    }
    PackUIStyleResources(style, packer);

    nlohmann::json style_json;
    style.SaveStyle(style_json);
    const auto& style_string_json = style_json.dump(2);
    packer.WriteFile(style_uri, "ui/style/", style_string_json.data(), style_string_json.size());

    window.SetStyleName(packer.MapUri(style_uri));

    // for each widget, parse the style string and see if there are more font-name props.
    window.ForEachWidget([&packer](uik::Widget* widget) {
        auto style_string = widget->GetStyleString();
        if (style_string.empty())
            return;
        DEBUG("Original widget style string. [widget='%1', style='%2']", widget->GetId(), style_string);
        engine::UIStyle style;
        style.ParseStyleString(widget->GetId(), style_string);

        PackUIStyleResources(style, packer);

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

        PackUIStyleResources(style, packer);

        window_style_string = style.MakeStyleString("window");
        // this is a bit of a hack, but we know that the style string
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
    for (size_t i=0; i<material.GetNumTextureMaps(); ++i)
    {
        auto* map = material.GetTextureMap(i);
        for (size_t j=0; j<map->GetNumTextures(); ++j)
        {
            auto* src = map->GetTextureSource(j);
            if (auto* text_source = dynamic_cast<gfx::detail::TextureTextBufferSource*>(src))
            {
                auto& text_buffer = text_source->GetTextBuffer();
                auto& text_chunk  = text_buffer.GetText();
                packer.CopyFile(text_chunk.font, "fonts/");
                text_chunk.font = packer.MapUri(text_chunk.font);
            }
            else if (auto* file_source = dynamic_cast<gfx::detail::TextureFileSource*>(src))
            {
                const auto& uri = file_source->GetFilename();
                packer.CopyFile(uri, "textures/");
                file_source->SetFileName(packer.MapUri(uri));
            }
        }
    }

    const auto& shader_glsl_uri = material.GetShaderUri();
    if (shader_glsl_uri.empty())
        return;
    const auto& shader_desc_uri = boost::replace_all_copy(shader_glsl_uri, ".glsl", ".json");
    // this only has significance when exporting/importing
    // a resource archive.
    packer.CopyFile(shader_desc_uri, "shaders/es2/");
    // copy the actual shader glsl
    packer.CopyFile(shader_glsl_uri, "shaders/es2/");
    material.SetShaderUri(packer.MapUri(shader_glsl_uri));
}

void MigrateResource(uik::Window& window, app::MigrationLog* log)
{
    // migration path for old data which doesn't yet have tab order values.
    const auto& keymap_uri = window.GetKeyMapFile();
    if (keymap_uri.empty())
    {
        window.SetKeyMapFile("app://ui/keymap/default.json");
        if (log)
        {
            log->Log(window, "UI", "Added default keymap file.");
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
        log->Log(window, "UI", "Generated UI widget tab order.");
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
