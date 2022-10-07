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

#include <unordered_map>

#include "base/assert.h"
#include "engine/ui.h"
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
} // namespace
} // namespace
