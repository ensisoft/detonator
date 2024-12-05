// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
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
#  include <base64/base64.h>
#include "warnpop.h"

#include <set>

#include "base/logging.h"
#include "base/format.h"
#include "base/hash.h"
#include "base/json.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/material.h"
#include "graphics/device.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "graphics/program.h"
#include "graphics/resource.h"
#include "graphics/loader.h"
#include "graphics/shadersource.h"
#include "graphics/algo.h"

#include "graphics/texture_map.cpp"
#include "graphics/texture_texture_source.cpp"
#include "graphics/texture_file_source.cpp"
#include "graphics/texture_bitmap_buffer_source.cpp"
#include "graphics/texture_bitmap_generator_source.cpp"
#include "graphics/texture_text_buffer_source.cpp"
#include "graphics/text_material.cpp"
#include "graphics/material_class.cpp"

//                  == Notes about shaders ==
// 1. Shaders are specific to a device within compatibility constraints
// but a GLES 2 device context cannot use GL ES 3 shaders for example.
//
// 2. Logically shaders are part of the higher level graphics system, i.e.
// they express some particular algorithm/technique in a device dependant
// way, so logically they belong to the higher layer (i.e. roughly painter
// material level of abstraction).
//
// Basically this means that the device abstraction breaks down because
// we need this device specific (shader) code that belongs to the higher
// level of the system.
//
// A fancy way of solving this would be to use a shader translator, i.e.
// a device that can transform shader from one language version to another
// language version or even from one shader language to another, for example
// from GLSL to HLSL/MSL/SPIR-V.  In fact this is exactly what libANGLE does
// when it implements GL ES2/3 on top of DX9/11/Vulkan/GL/Metal.
// But this only works when the particular underlying implementations can
// support similar feature sets. For example instanced rendering is not part
// of ES 2 but >= ES 3 so therefore it's not possible to translate ES3 shaders
// using instanced rendering into ES2 shaders directly but the whole rendering
// path must be changed to not use instanced rendering.
//
// This then means that if a system was using a shader translation layer (such
// as libANGLE) any decent system would still require several different rendering
// paths. For example a primary "high end path" and a "fallback path" for low end
// devices. These different paths would use different feature sets (such as instanced
// rendering) and also (in many cases) require different shaders to be written to
// fully take advantage of the graphics API features.
// Then these two different rendering paths would/could use a shader translation
// layer in order to use some specific graphics API. (through libANGLE)
//
//            <<Device>>
//              |    |
//     <ES2 Device> <ES3 Device>
//             |      |
//            <libANGLE>
//                 |
//      [GL/DX9/DX11/VULKAN/Metal]
//
// Where "ES2 Device" provides the low end graphics support and "ES3 Device"
// device provides the "high end" graphics rendering path support.
// Both device implementations would require their own shaders which would then need
// to be translated to the device specific shaders matching the feature level.
//
// Finally, who should own the shaders and who should edit them ?
//   a) The person who is using the graphics library ?
//   b) The person who is writing the graphics library ?
//
// The answer seems to be mostly the latter (b), i.e. in most cases the
// graphics functionality should work "out of the box" and the graphics
// library should *just work* without the user having to write shaders.
// However there's a need that user might want to write their own special
// shaders because they're cool to do so and want some special customized
// shader effect.
//

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
