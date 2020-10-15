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
        const std::string enum_name(magic_enum::enum_name(val));
        if (enum_name == name)
            return std::make_shared<gfx::MaterialClass>(gfx::SolidColor(gfx::Color4f(val)));
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
    if (name == "Rectangle")
        return std::make_shared<gfx::RectangleClass>();
    else if (name == "IsocelesTriangle")
        return std::make_shared<gfx::IsocelesTriangleClass>();
    else if (name == "RightTriangle")
        return std::make_shared<gfx::RightTriangleClass>();
    else if (name == "Circle")
        return std::make_shared<gfx::CircleClass>();
    else if (name == "RoundRect")
        return std::make_shared<gfx::RoundRectangleClass>();
    else if (name == "Trapezoid")
        return std::make_shared<gfx::TrapezoidClass>();
    else if (name == "Parallelogram")
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
    std::unordered_map<std::string, std::shared_ptr<Interface>>& out)
{
    if (!json.contains(type))
        return;
    for (const auto& iter : json[type].items())
    {
        auto& value = iter.value();
        const std::string& name = value["resource_name"];
        std::optional<Implementation> ret = Implementation::FromJson(value);
        if (!ret.has_value())
            throw std::runtime_error("Failed to load: " + type + "/" + name);

        out[name] = std::make_shared<Implementation>(std::move(ret.value()));
        //out[name] = std::make_shared<Implementation>(ret.value());
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

    game::LoadResources<gfx::MaterialClass, gfx::MaterialClass>(json, "materials", mMaterials);
    game::LoadResources<gfx::KinematicsParticleEngineClass, gfx::KinematicsParticleEngineClass>(json, "particles", mParticleEngines);
    game::LoadResources<gfx::PolygonClass, gfx::PolygonClass>(json, "shapes", mCustomShapes);
    game::LoadResources<AnimationClass, AnimationClass>(json, "animations", mAnimations);

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

const AnimationClass* ContentLoader::FindAnimationClass(const std::string& name) const
{
    auto it = mAnimations.find(name);
    if (it == std::end(mAnimations))
        return nullptr;
    return it->second.get();
}

std::unique_ptr<Animation> ContentLoader::CreateAnimation(const std::string& name) const
{
    auto it = mAnimations.find(name);
    if (it == std::end(mAnimations))
        return nullptr;
    return std::make_unique<Animation>(it->second);
}

} // namespace
