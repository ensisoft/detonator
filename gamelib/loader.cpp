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
#include "animation.h"
#include "loader.h"

namespace game
{

std::shared_ptr<const gfx::MaterialClass> ContentLoader::GetMaterialClass(const std::string& name) const
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

    ERROR("No such material class: '%1'", name);
    // for development purposes return some kind of valid object.
    return std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color::HotPink));
}

std::shared_ptr<const gfx::DrawableClass> ContentLoader::GetDrawableClass(const std::string& name) const
{
    // these are the current primitive cases that are not packed as part of the resources
    if (name == "_rect")
        return std::make_shared<gfx::RectangleClass>();
    else if (name == "_isosceles_triangle")
        return std::make_shared<gfx::IsocelesTriangleClass>();
    else if (name == "_right_triangle")
        return std::make_shared<gfx::RightTriangleClass>();
    else if (name == "_circle")
        return std::make_shared<gfx::CircleClass>();
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

    ERROR("No such drawable: '%1'", name);
    // for development purposes return some kind of valid object.
    return std::make_shared<gfx::RectangleClass>();
}

std::shared_ptr<gfx::Material> ContentLoader::MakeMaterial(const std::string& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClass(name));
}

std::shared_ptr<gfx::Drawable> ContentLoader::MakeDrawable(const std::string& name) const
{
    return gfx::CreateDrawableInstance(GetDrawableClass(name));
}

std::string ContentLoader::ResolveFile(gfx::ResourceLoader::ResourceType type, const std::string& file) const
{
    std::string str = file;
    if (str.find("pck://", 0) == 0)
        str = mResourceDir + "/" + str.substr(6);
    else if (str.find("ws://", 0) == 0)
        str = str.substr(5);

    //DEBUG("Mapped %1 to %2", file, str);
    return str;
}

template<typename Interface, typename Implementation>
void LoadResources(const nlohmann::json& json, const std::string& type,
    std::unordered_map<std::string, std::shared_ptr<Interface>>& out,
    std::unordered_map<std::string, std::string>& names)
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
        names[id] = name;
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

    game::LoadResources<gfx::MaterialClass, gfx::MaterialClass>(json, "materials", mMaterials, mNameTable);
    game::LoadResources<gfx::KinematicsParticleEngineClass, gfx::KinematicsParticleEngineClass>(json, "particles", mParticleEngines, mNameTable);
    game::LoadResources<gfx::PolygonClass, gfx::PolygonClass>(json, "shapes", mCustomShapes, mNameTable);
    game::LoadResources<AnimationClass, AnimationClass>(json, "animations", mAnimations, mNameTable);

    for (auto it = mAnimations.begin(); it != mAnimations.end();
        ++it)
    {
        const auto& name = it->first;
        AnimationClass* anim  = it->second.get();
        anim->Prepare(*this);
    }
    mResourceDir  = dir;
    mResourceFile = file;
}

const AnimationClass* ContentLoader::FindAnimationClassById(const std::string& id) const
{
    auto it = mAnimations.find(id);
    if (it == std::end(mAnimations))
        return nullptr;
    return it->second.get();
}

const AnimationClass* ContentLoader::FindAnimationClassByName(const std::string &name) const
{
    for (auto pair : mNameTable)
    {
        if (pair.second == name)
            return FindAnimationClassById(pair.first);
    }
    return nullptr;
}

std::unique_ptr<Animation> ContentLoader::CreateAnimationByName(const std::string& name) const
{
    for (auto pair : mNameTable)
    {
        if (pair.second == name)
            return CreateAnimationById(pair.first);
    }
    return nullptr;
}

std::unique_ptr<Animation> ContentLoader::CreateAnimationById(const std::string &id) const
{
    auto it = mAnimations.find(id);
    if (it == std::end(mAnimations))
        return nullptr;
    return game::CreateAnimationInstance(it->second);
}

} // namespace
