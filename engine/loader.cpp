// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <fstream>

#include "base/logging.h"
#include "base/platform.h"
#include "base/utility.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "engine/entity.h"
#include "engine/scene.h"
#include "engine/loader.h"

namespace game
{

ClassHandle<const gfx::MaterialClass> ContentLoader::FindMaterialClassById(const std::string& name) const
{
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        if ("_" + color_name == name)
        {
            auto ret = std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color4f(val)));
            ret->SetId("_" + color_name);
            return ret;
        }
    }

    auto it = mMaterials.find(name);
    if (it != std::end(mMaterials))
        return it->second;

    return nullptr;
}

ClassHandle<const gfx::DrawableClass> ContentLoader::FindDrawableClassById(const std::string& name) const
{
    // these are the current primitive cases that are not packed as part of the resources
    if (name == "_rect")
        return std::make_shared<gfx::RectangleClass>();
    else if (name == "_isosceles_triangle")
        return std::make_shared<gfx::IsoscelesTriangleClass>();
    else if (name == "_right_triangle")
        return std::make_shared<gfx::RightTriangleClass>();
    else if (name == "_capsule")
        return std::make_shared<gfx::CapsuleClass>();
    else if (name == "_circle")
        return std::make_shared<gfx::CircleClass>();
    else if (name == "_semi_circle")
        return std::make_shared<gfx::SemiCircleClass>();
    else if (name == "_round_rect")
        return std::make_shared<gfx::RoundRectangleClass>();
    else if (name == "_trapezoid")
        return std::make_shared<gfx::TrapezoidClass>();
    else if (name == "_parallelogram")
        return std::make_shared<gfx::ParallelogramClass>();

    // todo: perhaps need an identifier for the type of resource in question.
    // currently there's a name conflict that objects of different types but
    // with same names cannot be fully resolved by name only.

    {
        auto it = mParticleEngines.find(name);
        if (it != std::end(mParticleEngines))
            return it->second;
    }

    {
        auto it = mCustomShapes.find(name);
        if (it != std::end(mCustomShapes))
            return it->second;
    }

    return nullptr;
}

std::string ContentLoader::ResolveURI(gfx::ResourceLoader::ResourceType type, const std::string& URI) const
{
    auto it = mUriCache.find(URI);
    if (it != mUriCache.end())
        return it->second;

    std::string str = URI;
    if (str.find("pck://", 0) == 0)
        str = mResourceDir + "/" + str.substr(6);
    else if (str.find("ws://", 0) == 0)
        str = str.substr(5);
    else if (str.find("fs://", 0) == 0)
        str = str.substr(5);

    DEBUG("Mapped %1 to %2", URI, str);
    mUriCache[URI] = str;
    return str;
}

template<typename Interface, typename Implementation>
void LoadResources(const nlohmann::json& json, const std::string& type,
    std::unordered_map<std::string, std::shared_ptr<Interface>>& out,
    std::unordered_map<std::string, std::string>* namemap)
{
    if (!json.contains(type))
        return;
    for (const auto& iter : json[type].items())
    {
        auto& value = iter.value();
        const std::string& id   = value["resource_id"];
        const std::string& name = value["resource_name"];
        std::optional<Implementation> ret = Implementation::FromJson(value);
        if (!ret.has_value())
            throw std::runtime_error("Failed to load: " + type + "/" + name);

        out[id] = std::make_shared<Implementation>(std::move(ret.value()));
        if (namemap)
            (*namemap)[name] = id;
        DEBUG("Loaded '%1/%2'", type, name);
    }
}

void ContentLoader::LoadFromFile(const std::string& dir, const std::string& file)
{
    std::ifstream in(base::OpenBinaryInputStream(file));
    if (!in.is_open())
        throw std::runtime_error("failed to open: " + file);

    const std::string buffer(std::istreambuf_iterator<char>(in), {});
    // might throw.
    const auto& json = nlohmann::json::parse(buffer);

    game::LoadResources<gfx::MaterialClass, gfx::MaterialClass>(json, "materials", mMaterials, nullptr);
    game::LoadResources<gfx::KinematicsParticleEngineClass, gfx::KinematicsParticleEngineClass>(json, "particles", mParticleEngines, nullptr);
    game::LoadResources<gfx::PolygonClass, gfx::PolygonClass>(json, "shapes", mCustomShapes, nullptr);
    game::LoadResources<EntityClass, EntityClass>(json, "entities", mEntities, &mEntityNameTable);
    game::LoadResources<SceneClass, SceneClass>(json, "scenes", mScenes, &mSceneNameTable);

    // need to resolve the entity references.
    for (auto& p : mScenes)
    {
        auto& scene = p.second;
        for (size_t i=0; i<scene->GetNumNodes(); ++i)
        {
            auto& node = scene->GetNode(i);
            auto klass = FindEntityClassById(node.GetEntityId());
            if (!klass)
            {
                const auto& scene_name = mSceneNameTable[scene->GetId()];
                const auto& node_name  = node.GetName();
                const auto& node_entity_id = node.GetEntityId();
                ERROR("Scene node '%1/'%2'' refers to entity '%3' that is not found.",
                      scene_name, node_name, node_entity_id);
            }
            else
            {
                node.SetEntity(klass);
            }
        }
    }

    mResourceDir  = dir;
    mResourceFile = file;
}

ClassHandle<const game::EntityClass> ContentLoader::FindEntityClassByName(const std::string& name) const
{
    auto it = mEntityNameTable.find(name);
    if (it != mEntityNameTable.end())
        return FindEntityClassById(it->second);

    return nullptr;
}
ClassHandle<const game::EntityClass> ContentLoader::FindEntityClassById(const std::string& id) const
{
    auto it = mEntities.find(id);
    if (it != std::end(mEntities))
        return it->second;

    return nullptr;
}

ClassHandle<const SceneClass> ContentLoader::FindSceneClassByName(const std::string& name) const
{
    auto it = mSceneNameTable.find(name);
    if (it != mSceneNameTable.end())
        return FindSceneClassById(it->second);

    return nullptr;
}
ClassHandle<const SceneClass> ContentLoader::FindSceneClassById(const std::string& id) const
{
    auto it = mScenes.find(id);
    if (it != mScenes.end())
        return it->second;

    return nullptr;
}

} // namespace
