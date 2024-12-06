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

#include "graphics/material_class.h"
#include "graphics/material_instance.h"

namespace gfx
{

    // Create new material instance based on the given material class.
MaterialInstance::MaterialInstance(std::shared_ptr<const MaterialClass> klass, double time)
  : mClass(std::move(klass))
  , mRuntime(time)
{}
MaterialInstance::MaterialInstance(const MaterialClass& klass, double time)
  : mClass(klass.Copy())
  , mRuntime(time)
{}

bool MaterialInstance::ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.renderpass     = env.renderpass;
    state.material_time  = mRuntime;
    state.uniforms       = &mUniforms;
    state.first_render   = mFirstRender;

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
    return mClass->ApplyStaticState(state, device, program);
}

std::string MaterialInstance::GetShaderId(const Environment& env) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    return mClass->GetShaderId(state);
}

std::string MaterialInstance::GetShaderName(const Environment& env) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    return mClass->GetShaderName(state);
}

std::string MaterialInstance::GetClassId() const
{
    return mClass->GetId();
}


ShaderSource MaterialInstance::GetShader(const Environment& env, const Device& device) const
{
    MaterialClass::State state;
    state.editing_mode   = env.editing_mode;
    state.draw_primitive = env.draw_primitive;
    state.material_time  = mRuntime;
    state.uniforms       = &mUniforms;
    return mClass->GetShader(state, device);
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

std::unique_ptr<Material> CreateMaterialInstance(const std::shared_ptr<const MaterialClass>& klass)
{
    return std::make_unique<MaterialInstance>(klass);
}

} // namespace