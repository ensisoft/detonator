// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

#include "base/logging.h"
#include "graphics/material_class.h"
#include "graphics/material_instance.h"
#include "graphics/texture_texture_source.h"

namespace gfx
{

 // Create new material instance based on the given material class.
MaterialInstance::MaterialInstance(std::shared_ptr<const MaterialClass> klass, double time)
  : mClass(std::move(klass))
  , mRuntime(time)
{
    InitFlags();
}
MaterialInstance::MaterialInstance(const MaterialClass& klass, double time)
  : mClass(klass.Copy())
  , mRuntime(time)
{
    InitFlags();
}
MaterialInstance::MaterialInstance(MaterialClass&& klass, double time) noexcept
  : mClass(std::make_shared<MaterialClass>(std::move(klass)))
  , mRuntime(time)
{
    InitFlags();
}

bool MaterialInstance::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.draw_category  = env.draw_category;
    state.render_pass    = env.render_pass;
    state.material_time  = mRuntime;
    state.uniforms       = &mUniforms;
    state.first_render   = mFirstRender;
    state.flags          = mFlags;;

    if (mSpriteCycle.has_value())
    {
        const auto& cycle = mSpriteCycle.value();
        if (cycle.started)
        {
            state.active_texture_map_id  = cycle.id;
            state.material_time = cycle.runtime;
        }
    }
    else
    {
        // todo: refactor this away from *uniforms*
        if (const auto* active_texture = base::SafeFind(mUniforms, "active_texture_map"))
        {
            if (const auto* id = std::get_if<std::string>(active_texture))
                state.active_texture_map_id = *id;
            else if (mFirstRender)
                WARN("Incorrect material parameter type set on 'active_texture_map'. String ID expected.");
        }
    }
    if (!mStaticUniformWarning)
    {
        if (mClass->IsStatic() && !mUniforms.empty())
        {
            WARN("Trying to set material uniforms on a static material. [name='%1']", mClass->GetName());
            mStaticUniformWarning = true;
        }
    }

    mFirstRender = false;

    if (!mClass->ApplyDynamicState(state, device, program))
    {
        mError = true;
        return false;
    }
    mError = false;

    const auto surface = mClass->GetSurfaceType();
    if (surface == MaterialClass::SurfaceType::Opaque)
        raster.blending = RasterState::Blending::None;
    else if (surface == MaterialClass::SurfaceType::Transparent)
        raster.blending = RasterState::Blending::Transparent;
    else if (surface == MaterialClass::SurfaceType::Emissive)
        raster.blending = RasterState::Blending::Additive;

    raster.premultiplied_alpha = mClass->PremultipliedAlpha();
    return true;
}

void MaterialInstance::ApplyStaticState(const Environment& env, Device& device, ProgramState& program) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.draw_category  = env.draw_category;
    return mClass->ApplyStaticState(state, device, program);
}

std::string MaterialInstance::GetShaderId(const Environment& env) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.draw_category  = env.draw_category;
    return mClass->GetShaderId(state);
}

std::string MaterialInstance::GetShaderName(const Environment& env) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.draw_category  = env.draw_category;
    return mClass->GetShaderName(state);
}

std::string MaterialInstance::GetClassId() const
{
    return mClass->GetId();
}

bool MaterialInstance::Execute(const Environment& env, const Command& cmd)
{
    if (cmd.name != "RunSpriteCycle")
        return false;
    if (mClass->GetType() != gfx::MaterialClass::Type::Sprite)
        return false;
    if (mSpriteCycle.has_value())
        return false;

    const auto* sprite_cycle_id = base::SafeFind(cmd.args, std::string("id"));
    if (!sprite_cycle_id || !std::holds_alternative<std::string>(*sprite_cycle_id))
        return false;

    const auto* texture_map = mClass->FindTextureMapById(std::get<std::string>(*sprite_cycle_id));
    if (!texture_map)
        return false;

    float delay = 0.0f;
    if (const auto* p = base::SafeFind(cmd.args, "delay"))
    {
        if (std::holds_alternative<float>(*p))
            delay = std::get<float>(*p);
    }
    else
    {
        const TextureMap* map = nullptr;
        if (const auto* active = base::SafeFind(mUniforms, "active_texture_map"))
        {
            if (std::holds_alternative<std::string>(*active))
                map = mClass->FindTextureMapById(std::get<std::string>(*active));
        }
        else
        {
            const auto& id = mClass->GetActiveTextureMap();
            map = mClass->FindTextureMapById(id);
        }
        if (!map || !map->IsSpriteMap())
            return false;

        const auto d = map->GetSpriteCycleDuration();
        if (map->IsSpriteLooping())
        {
            const auto t = fmod(mRuntime, d);
            delay = d - t;
        }
        else
        {
            if (mRuntime < d)
                delay = d - mRuntime;
        }
    }

    SpriteCycleRun cycle;
    cycle.name    = texture_map->GetName();
    cycle.id      = std::get<std::string>(*sprite_cycle_id);
    cycle.delay   = delay;
    cycle.runtime = 0.0;
    cycle.started = false;
    mSpriteCycle  = cycle;
    return true;

}

void MaterialInstance::Update(float dt)
{
    const auto t = mRuntime;

    mRuntime += dt;

    if (!mSpriteCycle.has_value())
        return;

    auto& cycle = mSpriteCycle.value();
    if (cycle.started)
    {
        cycle.runtime += dt;

        const auto* map = mClass->FindTextureMapById(cycle.id);
        if (cycle.runtime >= map->GetSpriteCycleDuration())
            mSpriteCycle.reset();
    }
    else
    {
        cycle.delay -= dt;
        if (cycle.delay <= 0.0f)
            cycle.started = true;
    }
}

void MaterialInstance::SetRuntime(double runtime)
{
    if (runtime > mRuntime)
    {
        const auto dt = runtime - mRuntime;
        Update(dt);
    }
    else
    {
        mRuntime = runtime;
    }
}

bool MaterialInstance::GetValue(const std::string& key, RuntimeValue* value) const
{
    if (key == "SpriteCycleTime" && mSpriteCycle.has_value())
    {
        *value = mSpriteCycle->runtime;
        return true;
    }
    else if (key == "SpriteCycleName" && mSpriteCycle.has_value())
    {
        *value = mSpriteCycle->name;
        return true;
    }
    return false;
}

ShaderSource MaterialInstance::GetShader(const Environment& env, const Device& device) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.draw_category  = env.draw_category;
    state.material_time  = mRuntime;
    state.uniforms       = &mUniforms;
    return mClass->GetShader(state, device);
}

void MaterialInstance::InitFlags() noexcept
{
    SetFlag(Flags::EnableBloom, mClass->TestFlag(MaterialClass::Flags::EnableBloom));
    SetFlag(Flags::EnableLight, mClass->TestFlag(MaterialClass::Flags::EnableLight));
}

std::unique_ptr<Material> MaterialInstance::Clone() const
{
    auto dolly = std::make_unique<MaterialInstance>(*this);
    dolly->mFirstRender = false;
    dolly->mError = false;
    dolly->mStaticUniformWarning = false;
    return dolly;
}

MaterialInstance CreateMaterialFromColor(const Color4f& top_left,
                                         const Color4f& top_right,
                                         const Color4f& bottom_left,
                                         const Color4f& bottom_right)
{
    return MaterialInstance(CreateMaterialClassFromColor(top_left, top_right, bottom_left, bottom_right));
}

MaterialInstance CreateMaterialFromColor(const Color4f& color)
{
    return MaterialInstance(CreateMaterialClassFromColor(color));
}

MaterialInstance CreateMaterialFromImage(const std::string& uri)
{
    return MaterialInstance(CreateMaterialClassFromImage(uri));
}
MaterialInstance CreateMaterialFromSprite(const std::string& uri)
{
    return MaterialInstance(CreateMaterialClassFromSprite(uri));
}

MaterialInstance CreateMaterialFromImages(const std::initializer_list<std::string>& uris)
{
    return MaterialInstance(CreateMaterialClassFromImages(uris));
}

MaterialInstance CreateMaterialFromImages(const std::vector<std::string>& uris)
{
    return MaterialInstance(CreateMaterialClassFromImages(uris));
}

MaterialInstance CreateMaterialFromSpriteAtlas(const std::string& uri, const std::vector<FRect>& frames)
{
    return MaterialInstance(CreateMaterialClassFromSpriteAtlas(uri, frames));
}

MaterialInstance CreateMaterialFromText(const TextBuffer& text)
{
    return MaterialInstance(CreateMaterialClassFromText(text));
}

MaterialInstance CreateMaterialFromText(TextBuffer&& text)
{
    return MaterialInstance(CreateMaterialClassFromText(std::move(text)));
}

MaterialInstance CreateMaterialFromTexture(std::string gpu_id, Texture* texture)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetType(TextureMap::Type::Texture2D);
    map->SetName("Texture");
    map->SetNumTextures(1);
    map->SetTextureSource(0, UseExistingTexture(std::move(gpu_id), texture));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));

    return MaterialInstance(material);
}

std::unique_ptr<Material> CreateMaterialInstance(const MaterialClass& klass)
{
    return std::make_unique<MaterialInstance>(klass);
}

std::unique_ptr<Material> CreateMaterialInstance(MaterialClass&& klass)
{
    return std::make_unique<MaterialInstance>(std::move(klass));
}

std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass)
{
    return std::make_unique<MaterialInstance>(klass);
}

} // namespace