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

#include <set>

#include "base/logging.h"
#include "base/format.h"
#include "base/hash.h"
#include "data/reader.h"
#include "data/writer.h"
#include "graphics/program.h"
#include "graphics/shader_source.h"
#include "graphics/material_class.h"
#include "graphics/texture_source.h"
#include "graphics/texture_texture_source.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_bitmap_buffer_source.h"
#include "graphics/texture_bitmap_generator_source.h"
#include "graphics/texture_text_buffer_source.h"
#include "graphics/packer.h"
#include "graphics/loader.h"
#include "graphics/enum.h"

namespace {
    enum class BasicLightMaterialMap : int {
        Diffuse = 0x1, Specular = 0x2, Normal = 0x4
    };
} // namesapce

namespace gfx
{

MaterialClass::MaterialClass(Type type, std::string id)
  : mClassId(std::move(id))
  , mType(type)
{
    mFlags.set(Flags::BlendFrames, true);
    mFlags.set(Flags::EnableBloom, true);
    mFlags.set(Flags::EnableLight, true);
}

MaterialClass::MaterialClass(const MaterialClass& other, bool copy)
{
    mClassId          = copy ? other.mClassId : base::RandomString(10);
    mName             = other.mName;
    mType             = other.mType;
    mFlags            = other.mFlags;
    mShaderUri        = other.mShaderUri;
    mShaderSrc        = other.mShaderSrc;
    mActiveTextureMap = other.mActiveTextureMap;
    mSurfaceType      = other.mSurfaceType;
    mTextureMinFilter = other.mTextureMinFilter;
    mTextureMagFilter = other.mTextureMagFilter;
    mTextureWrapX     = other.mTextureWrapX;
    mTextureWrapY     = other.mTextureWrapY;
    mUniforms         = other.mUniforms;

    for (const auto& src : other.mTextureMaps)
    {
        auto map = copy ? src->Copy() : src->Clone();
        if (src->GetId() == other.mActiveTextureMap)
        {
            mActiveTextureMap = map->GetId();
        }
        mTextureMaps.push_back(std::move(map));
    }
}

MaterialClass::MaterialClass(MaterialClass&& other) noexcept
{
    mClassId = std::move(other.mClassId);
    mName = std::move(other.mName);
    mType = other.mType;
    mFlags = other.mFlags;
    mShaderUri = std::move(other.mShaderUri);
    mShaderSrc = std::move(other.mShaderSrc);
    mActiveTextureMap = std::move(other.mActiveTextureMap);
    mSurfaceType = other.mSurfaceType;
    mTextureMinFilter = other.mTextureMinFilter;
    mTextureMagFilter = other.mTextureMagFilter;
    mTextureWrapX = other.mTextureWrapX;
    mTextureWrapY = other.mTextureWrapY;
    mUniforms = std::move(other.mUniforms);
    mTextureMaps = std::move(other.mTextureMaps);
}

MaterialClass::~MaterialClass() = default;

std::string MaterialClass::GetShaderName(const State& state) const noexcept
{
    if (mType == Type::Custom)
        return mName;
    if (IsStatic())
        return mName;
    return base::FormatString("%1 Shader", mType);
}

size_t MaterialClass::GetShaderHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, mShaderUri);

    if (mType == Type::Color)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetBaseColor());
        }
    }
    else if (mType == Type::Gradient)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor0));
            hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor1));
            hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor2));
            hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor3));
            hash = base::hash_combine(hash, GetGradientWeight());
            hash = base::hash_combine(hash, mSurfaceType);
        }
    }
    else if (mType == Type::Sprite)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetBaseColor());
            hash = base::hash_combine(hash, GetTextureScale());
            hash = base::hash_combine(hash, GetTextureVelocity());
            hash = base::hash_combine(hash, GetTextureRotation());
            hash = base::hash_combine(hash, GetAlphaCutoff());
            hash = base::hash_combine(hash, mSurfaceType);
        }
    }
    else if (mType == Type::Texture)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetBaseColor());
            hash = base::hash_combine(hash, GetTextureScale());
            hash = base::hash_combine(hash, GetTextureVelocity());
            hash = base::hash_combine(hash, GetTextureRotation());
            hash = base::hash_combine(hash, GetAlphaCutoff());
            hash = base::hash_combine(hash, mSurfaceType);
        }
    }
    else if (mType == Type::Tilemap)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetBaseColor());
            hash = base::hash_combine(hash, GetAlphaCutoff());
            hash = base::hash_combine(hash, GetTileSize());
            hash = base::hash_combine(hash, GetTileOffset());
            hash = base::hash_combine(hash, mSurfaceType);
        }
    }
    else if (mType == Type::Particle2D)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetParticleStartColor());
            hash = base::hash_combine(hash, GetParticleEndColor());
            hash = base::hash_combine(hash, GetParticleBaseRotation());
            hash = base::hash_combine(hash, mSurfaceType);
        }
    }
    else if (mType == Type::BasicLight)
    {
        if (IsStatic())
        {
            hash = base::hash_combine(hash, GetAmbientColor());
            hash = base::hash_combine(hash, GetDiffuseColor());
            hash = base::hash_combine(hash, GetSpecularColor());
            hash = base::hash_combine(hash, GetSpecularExponent());
        }
    }
    else if (mType == Type::Custom)
    {
        // todo: static uniform information

    } else BUG("Unknown material type.");

    return hash;
}

std::string MaterialClass::GetShaderId(const State& state) const noexcept
{
    size_t hash = 0;
    if (mCache.has_value())
    {
        hash = mCache.value().shader_hash;
    }
    else
    {
        hash = GetShaderHash();
    }
    hash = base::hash_combine(hash, state.draw_primitive);
    hash = base::hash_combine(hash, state.draw_category);
    return base::FormatString("%1+%2", mType, hash);
}

std::size_t MaterialClass::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mClassId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mShaderUri);
    hash = base::hash_combine(hash, mShaderSrc);
    hash = base::hash_combine(hash, mActiveTextureMap);
    hash = base::hash_combine(hash, mSurfaceType);
    hash = base::hash_combine(hash, mTextureMinFilter);
    hash = base::hash_combine(hash, mTextureMagFilter);
    hash = base::hash_combine(hash, mTextureWrapX);
    hash = base::hash_combine(hash, mTextureWrapY);
    hash = base::hash_combine(hash, mFlags);

    hash = base::hash_combine(hash, GetTextureRotation());
    hash = base::hash_combine(hash, GetTextureScale());
    hash = base::hash_combine(hash, GetTextureVelocity());
    hash = base::hash_combine(hash, GetColor(ColorIndex::BaseColor));
    hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor0));
    hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor1));
    hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor2));
    hash = base::hash_combine(hash, GetColor(ColorIndex::GradientColor3));
    hash = base::hash_combine(hash, GetGradientWeight());
    hash = base::hash_combine(hash, GetAlphaCutoff());
    hash = base::hash_combine(hash, GetTileSize());
    hash = base::hash_combine(hash, GetTileOffset());
    hash = base::hash_combine(hash, GetParticleStartColor());
    hash = base::hash_combine(hash, GetParticleEndColor());
    hash = base::hash_combine(hash, GetParticleRotation());
    hash = base::hash_combine(hash, GetParticleBaseRotation());

    hash = base::hash_combine(hash, GetAmbientColor());
    hash = base::hash_combine(hash, GetDiffuseColor());
    hash = base::hash_combine(hash, GetSpecularColor());
    hash = base::hash_combine(hash, GetSpecularExponent());

    // remember that the order of uniforms (and texturemaps)
    // can change between IntoJson/FromJson! This can result
    // in a different order of items in the *unordered* maps !
    // This then can result in a different hash value being
    // computed. To avoid this non-sense problem use a set
    // to give a consistent iteration order over the uniforms
    // and texture maps.
    std::set<std::string> keys;
    for (const auto& uniform : mUniforms)
        keys.insert(uniform.first);

    for (const auto& key : keys)
    {
        const auto* uniform = base::SafeFind(mUniforms, key);
        hash = base::hash_combine(hash, key);
        hash = base::hash_combine(hash, *uniform);
    }

    for (const auto& map : mTextureMaps)
    {
        hash = base::hash_combine(hash, map->GetHash());
    }
    return hash;
}

ShaderSource MaterialClass::GetShader(const State& state, const Device& device) const noexcept
{
    auto source = GetShaderSource(state, device);
    if (source.IsEmpty())
        return source;

    if (!source.HasShaderBlock("PI", ShaderSource::ShaderBlockType::PreprocessorDefine))
        source.AddPreprocessorDefinition("PI", "3.1415926");

    source.AddPreprocessorDefinition("MATERIAL_SURFACE_TYPE_OPAQUE",      static_cast<unsigned>(SurfaceType::Opaque));
    source.AddPreprocessorDefinition("MATERIAL_SURFACE_TYPE_TRANSPARENT", static_cast<unsigned>(SurfaceType::Transparent));
    source.AddPreprocessorDefinition("MATERIAL_SURFACE_TYPE_EMISSIVE",    static_cast<unsigned>(SurfaceType::Emissive));
    source.AddPreprocessorDefinition("TEXTURE_WRAP_CLAMP",  static_cast<int>(TextureWrapping::Clamp));
    source.AddPreprocessorDefinition("TEXTURE_WRAP_REPEAT", static_cast<int>(TextureWrapping::Repeat));
    source.AddPreprocessorDefinition("TEXTURE_WRAP_MIRROR", static_cast<int>(TextureWrapping::Mirror));

    source.AddPreprocessorDefinition("MATERIAL_FLAGS_ENABLE_BLOOM", static_cast<unsigned>(MaterialFlags::EnableBloom));
    source.AddPreprocessorDefinition("MATERIAL_FLAGS_ENABLE_LIGHT", static_cast<unsigned>(MaterialFlags::EnableLight));

    if (IsBuiltIn())
    {
        source.AddPreprocessorDefinition("PARTICLE_EFFECT_NONE",   static_cast<int>(ParticleEffect::None));
        source.AddPreprocessorDefinition("PARTICLE_EFFECT_ROTATE", static_cast<int>(ParticleEffect::Rotate));
        if (mType == Type::Particle2D)
        {
            source.AddPreprocessorDefinition("PARTICLE_ROTATION_NONE", static_cast<unsigned>(ParticleRotation::None));
            source.AddPreprocessorDefinition("PARTICLE_ROTATION_BASE", static_cast<unsigned>(ParticleRotation::BaseRotation));
            source.AddPreprocessorDefinition("PARTICLE_ROTATION_RANDOM", static_cast<unsigned>(ParticleRotation::RandomRotation));
            source.AddPreprocessorDefinition("PARTICLE_ROTATION_DIRECTION", static_cast<unsigned>(ParticleRotation::ParticleDirection));
            source.AddPreprocessorDefinition("PARTICLE_ROTATION_DIRECTION_AND_BASE", static_cast<unsigned>(ParticleRotation::ParticleDirectionAndBase));
        }
        else if (mType == Type::BasicLight)
        {
            source.AddPreprocessorDefinition("BASIC_LIGHT_MATERIAL_DIFFUSE_MAP",  static_cast<unsigned>(BasicLightMaterialMap::Diffuse));
            source.AddPreprocessorDefinition("BASIC_LIGHT_MATERIAL_SPECULAR_MAP", static_cast<unsigned>(BasicLightMaterialMap::Specular));
            source.AddPreprocessorDefinition("BASIC_LIGHT_MATERIAL_NORMAL_MAP",   static_cast<unsigned>(BasicLightMaterialMap::Normal));
        }
        else if (mType == Type::Gradient)
        {
            source.AddPreprocessorDefinition("GRADIENT_TYPE_BILINEAR", static_cast<unsigned>(GradientType::Bilinear));
            source.AddPreprocessorDefinition("GRADIENT_TYPE_RADIAL", static_cast<unsigned>(GradientType::Radial));
            source.AddPreprocessorDefinition("GRADIENT_TYPE_CONICAL", static_cast<unsigned>(GradientType::Conical));
        }
    }

    if (state.draw_primitive == DrawPrimitive::Triangles)
        source.AddPreprocessorDefinition("DRAW_TRIANGLES");
    else if (state.draw_primitive == DrawPrimitive::Points)
        source.AddPreprocessorDefinition("DRAW_POINTS");
    else if (state.draw_primitive == DrawPrimitive::Lines)
        source.AddPreprocessorDefinition("DRAW_LINES");
    else BUG("Bug on draw primitive");

    if (state.draw_category == DrawCategory::Particles)
        source.AddPreprocessorDefinition("GEOMETRY_IS_PARTICLES");
    else if (state.draw_category == DrawCategory::TileBatch)
        source.AddPreprocessorDefinition("GEOMETRY_IS_TILES");
    else if (state.draw_category == DrawCategory::Basic)
        source.AddPreprocessorDefinition("GEOMETRY_IS_BASIC");
    else BUG("Bug on draw category");


    if (IsStatic())
    {
        source.AddPreprocessorDefinition("STATIC_SHADER_SOURCE");

        if (mSurfaceType == SurfaceType::Transparent)
            source.AddPreprocessorDefinition("TRANSPARENT_SURFACE");
        else if (mSurfaceType == SurfaceType::Opaque)
            source.AddPreprocessorDefinition("OPAQUE_SURFACE");
        else if (mSurfaceType == SurfaceType::Emissive)
            source.AddPreprocessorDefinition("EMISSIVE_SURFACE");
        else BUG("Bug on surface type");

        if (IsBuiltIn())
        {
            // fold a set of known uniforms to constants in the shader
            // code so that we don't need to set them at runtime.
            // the tradeoff is that this creates more shader programs!
            source.FoldUniform("kAlphaCutoff", GetAlphaCutoff());
            source.FoldUniform("kBaseColor", GetColor(ColorIndex::BaseColor));
            source.FoldUniform("kGradientColor0", GetColor(ColorIndex::GradientColor0));
            source.FoldUniform("kGradientColor1", GetColor(ColorIndex::GradientColor1));
            source.FoldUniform("kGradientColor2", GetColor(ColorIndex::GradientColor2));
            source.FoldUniform("kGradientColor3", GetColor(ColorIndex::GradientColor3));
            source.FoldUniform("kGradientGamma", GetGradientGamma());
            source.FoldUniform("kGradientWeight", GetGradientWeight());
            source.FoldUniform("kGradientType", static_cast<unsigned>(GetGradientType()));
            source.FoldUniform("kTextureVelocity", GetTextureVelocity());
            source.FoldUniform("kTextureVelocityXY", glm::vec2(GetTextureVelocity()));
            source.FoldUniform("kTextureVelocityZ", GetTextureVelocity().z);
            source.FoldUniform("kTextureRotation", GetTextureRotation());
            source.FoldUniform("kTextureScale", GetTextureScale());
            source.FoldUniform("kTileSize", GetTileSize());
            source.FoldUniform("kTileOffset", GetTileOffset());
            source.FoldUniform("kTilePadding", GetTilePadding());
            source.FoldUniform("kSurfaceType", static_cast<unsigned>(mSurfaceType));
            source.FoldUniform("kParticleStartColor", GetParticleStartColor());
            source.FoldUniform("kParticleEndColor", GetParticleEndColor());
            source.FoldUniform("kParticleMidColor", GetParticleMidColor());
            source.FoldUniform("kParticleBaseRotation", GetParticleBaseRotation());

            source.FoldUniform("kAmbientColor", GetAmbientColor());
            source.FoldUniform("kDiffuseColor", GetDiffuseColor());
            source.FoldUniform("kSpecularColor", GetSpecularColor());
            source.FoldUniform("kSpecularExponent", GetSpecularExponent());
        }
    }
    else
    {
        source.AddPreprocessorDefinition("DYNAMIC_SHADER_SOURCE");
    }
    return source;
}

bool MaterialClass::ApplyDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    program.SetUniform("kTime", (float)state.material_time);
    program.SetUniform("kEditingMode", (int)state.editing_mode);
    program.SetUniform("kSurfaceType", static_cast<unsigned>(mSurfaceType));
    program.SetUniform("kMaterialFlags", static_cast<unsigned>(state.flags));

    // for the future... for different render passes we got two options
    // either the single shader implements the different render pass
    // functionality or then there are different shaders for different passes
    // program.SetUniform("kRenderPass", (int)state.renderpass);

    if (mType == Type::Color)
    {
        if (!IsStatic())
        {
            SetUniform("kBaseColor", state.uniforms, GetBaseColor(),   program);
        }
    }
    else if (mType == Type::Gradient)
    {
        if (!IsStatic())
        {
            SetUniform("kGradientColor0", state.uniforms, GetColor(ColorIndex::GradientColor0), program);
            SetUniform("kGradientColor1", state.uniforms, GetColor(ColorIndex::GradientColor1), program);
            SetUniform("kGradientColor2", state.uniforms, GetColor(ColorIndex::GradientColor2), program);
            SetUniform("kGradientColor3", state.uniforms, GetColor(ColorIndex::GradientColor3), program);
            SetUniform("kGradientWeight", state.uniforms, GetGradientWeight(), program);
            SetUniform("kGradientGamma",  state.uniforms, GetGradientGamma(), program);
            SetUniform("kGradientType", state.uniforms, static_cast<unsigned>(GetGradientType()), program);
        }
    }
    else if (mType == Type::Sprite)
        return ApplySpriteDynamicState(state, device, program);
    else if (mType == Type::Texture)
        return ApplyTextureDynamicState(state, device, program);
    else if (mType == Type::Tilemap)
        return ApplyTilemapDynamicState(state, device, program);
    else if (mType == Type::Particle2D)
        return ApplyParticleDynamicState(state, device, program);
    else if (mType == Type::BasicLight)
        return ApplyBasicLightDynamicState(state, device, program);
    else if (mType == Type::Custom)
        return ApplyCustomDynamicState(state, device, program);
    else BUG("Unknown material type.");

    return true;
}

void MaterialClass::ApplyStaticState(const State& state, Device& device, ProgramState& program) const noexcept
{
    if (mType == Type::Color)
    {
        program.SetUniform("kBaseColor", GetBaseColor());
    }
    else if (mType == Type::Gradient)
    {
        program.SetUniform("kGradientColor0", GetColor(ColorIndex::GradientColor0));
        program.SetUniform("kGradientColor1", GetColor(ColorIndex::GradientColor1));
        program.SetUniform("kGradientColor2", GetColor(ColorIndex::GradientColor2));
        program.SetUniform("kGradientColor3", GetColor(ColorIndex::GradientColor3));
        program.SetUniform("kGradientWeight", GetGradientWeight());
        program.SetUniform("kGradientGamma",  GetGradientGamma());
        program.SetUniform("kGradientType", static_cast<unsigned>(GetGradientType()));
    }
    else if (mType == Type::Sprite)
    {
        program.SetUniform("kBaseColor",         GetBaseColor());
        program.SetUniform("kTextureScale",      GetTextureScale());
        program.SetUniform("kTextureVelocity",   GetTextureVelocity());
        program.SetUniform("kTextureRotation",   GetTextureRotation());
        program.SetUniform("kAlphaCutoff",       GetAlphaCutoff());
    }
    else if (mType == Type::Texture)
    {
        program.SetUniform("kBaseColor",         GetBaseColor());
        program.SetUniform("kTextureScale",      GetTextureScale());
        program.SetUniform("kTextureVelocity",   GetTextureVelocity());
        program.SetUniform("kTextureRotation",   GetTextureRotation());
        program.SetUniform("kAlphaCutoff",       GetAlphaCutoff());
    }
    else if (mType == Type::Tilemap)
    {
        program.SetUniform("kBaseColor",   GetBaseColor());
        program.SetUniform("kAlphaCutoff", GetAlphaCutoff());
        // I'm not sure if there's a use case for setting the texture
        // scale or texture velocity. so we're not applying these now.
    }
    else if (mType ==Type::Particle2D)
    {
        program.SetUniform("kParticleStartColor",   GetParticleStartColor());
        program.SetUniform("kParticleEndColor",     GetParticleEndColor());
        program.SetUniform("kParticleMidColor",     GetParticleMidColor());
        program.SetUniform("kParticleBaseRotation", GetParticleBaseRotation());
    }
    else if (mType == Type::BasicLight)
    {
        program.SetUniform("kAmbientColor", GetAmbientColor());
        program.SetUniform("kDiffuseColor", GetDiffuseColor());
        program.SetUniform("kSpecularColor", GetSpecularColor());
        program.SetUniform("kSpecularExponent", GetSpecularExponent());
    }

    else if (mType == Type::Custom)
    {
        // nothing to do here, static state should be in the shader
        // already, either by the shader programmer or by the shader
        // source generator.
    } else BUG("Unknown material type.");
}

void MaterialClass::IntoJson(data::Writer& data) const
{
    data.Write("type",               mType);
    data.Write("id",                 mClassId);
    data.Write("name",               mName);
    data.Write("shader_uri",         mShaderUri);
    data.Write("shader_src",         mShaderSrc);
    data.Write("active_texture_map", mActiveTextureMap);
    data.Write("surface",            mSurfaceType);
    data.Write("texture_min_filter", mTextureMinFilter);
    data.Write("texture_mag_filter", mTextureMagFilter);
    data.Write("texture_wrap_x",     mTextureWrapX);
    data.Write("texture_wrap_y",     mTextureWrapY);
    data.Write("flags",              mFlags);

    // use an ordered set for persisting the data to make sure
    // that the order in which the uniforms are written out is
    // defined in order to avoid unnecessary changes (as perceived
    // by a version control such as Git) when there's no actual
    // change in the data.
    std::set<std::string> uniform_keys;
    for (const auto& uniform : mUniforms)
        uniform_keys.insert(uniform.first);

    for (const auto& key : uniform_keys)
    {
        const auto& uniform = *base::SafeFind(mUniforms, key);
        auto chunk = data.NewWriteChunk();
        chunk->Write("name", key);
        std::visit([&chunk](const auto& variant_value) {
            chunk->Write("value", variant_value);
        }, uniform);
        data.AppendChunk("uniforms", std::move(chunk));
    }

    for (const auto& map : mTextureMaps)
    {
        auto chunk = data.NewWriteChunk();
        map->IntoJson(*chunk);
        ASSERT(chunk->HasValue("name")); // moved into TextureMap::IntoJson
        ASSERT(chunk->HasValue("type")); // moved into TextureMap::IntoJson
        //chunk->Write("name", key);
        //chunk->Write("type", map->GetType());
        data.AppendChunk("texture_maps", std::move(chunk));
    }
}

template<typename T> // static
bool MaterialClass::SetUniform(const char* name, const UniformMap* uniforms, const T& backup, ProgramState& program)
{
    if (uniforms)
    {
        auto it = uniforms->find(name);
        if (it == uniforms->end())
        {
            program.SetUniform(name, backup);
            return true;
        }
        const auto& value = it->second;
        if (const auto* ptr = std::get_if<T>(&value))
        {
            program.SetUniform(name, *ptr);
            return true;
        }
    }
    program.SetUniform(name, backup);
    return false;
}

// static
bool MaterialClass::SetUniform(const char* name, const UniformMap* uniforms, unsigned backup, ProgramState& program)
{
    if (uniforms)
    {
        auto it = uniforms->find(name);
        if (it == uniforms->end())
        {
            program.SetUniform(name, backup);
            return true;
        }
        const auto& value = it->second;
        // right now we're only exposing int type in the supported uniforms.
        // adding unsigned requires all the layers above to change too and
        // make sure they deal with int vs unsigned int properly. Including
        // Lua scripting and UI layers...
        // But since we're only using this as a flag type it should be fine
        // for now.
        if (const auto* ptr = std::get_if<int>(&value))
        {
            if (*ptr > 0)
            {
                program.SetUniform(name, *ptr);
                return true;
            }
        }
    }
    program.SetUniform(name, backup);
    return false;
}

template<typename T>
bool MaterialClass::ReadLegacyValue(const char* name, const char* uniform, const data::Reader& data)
{
    if (!data.HasValue(name))
        return true;

    T the_old_value;
    if (!data.Read(name, &the_old_value))
        return false;

    SetUniform(uniform, the_old_value);
    return true;
}

bool MaterialClass::FromJson(const data::Reader& data, unsigned flags)
{
    bool ok = true;
    ok &= data.Read("type",               &mType);
    ok &= data.Read("id",                 &mClassId);
    ok &= data.Read("name",               &mName);
    ok &= data.Read("shader_uri",         &mShaderUri);
    ok &= data.Read("shader_src",         &mShaderSrc);
    ok &= data.Read("active_texture_map", &mActiveTextureMap);
    ok &= data.Read("surface",            &mSurfaceType);
    ok &= data.Read("texture_min_filter", &mTextureMinFilter);
    ok &= data.Read("texture_mag_filter", &mTextureMagFilter);
    ok &= data.Read("texture_wrap_x",     &mTextureWrapX);
    ok &= data.Read("texture_wrap_y",     &mTextureWrapY);
    ok &= data.Read("flags",              &mFlags);

    // these member variables have been folded into the generic uniform map.
    // this is the old way they were written out and this code migrates the
    // old materials from member variables -> uniform map.
    ok &= ReadLegacyValue<Color4f>("color_map0", "kColor0", data);
    ok &= ReadLegacyValue<Color4f>("color_map1", "kColor1", data);
    ok &= ReadLegacyValue<Color4f>("color_map2", "kColor2", data);
    ok &= ReadLegacyValue<Color4f>("color_map3", "kColor3", data);
    ok &= ReadLegacyValue<glm::vec2>("color_weight", "kGradientWeight", data);
    ok &= ReadLegacyValue<glm::vec2>("texture_scale", "kTextureScale", data);
    ok &= ReadLegacyValue<glm::vec3>("texture_velocity", "kTextureVelocity", data);
    ok &= ReadLegacyValue<float>("texture_rotation", "kTextureRotation", data);

    if (data.HasValue("particle_action"))
    {
        // migrated to use a uniform
        ParticleEffect effect;
        ok &= data.Read("particle_action", &effect);
        if (effect != ParticleEffect::None)
            SetParticleEffect(effect);
    }

    if (data.HasValue("static"))
    {
        bool static_content = false;
        ok &= data.Read("static", &static_content);
        mFlags.set(Flags::Static, static_content);
    }

    if (mType == Type::Color)
    {
        if (data.HasValue("color"))
            ok &= ReadLegacyValue<Color4f>("color", "kBaseColor", data);
    }
    else if (mType == Type::Gradient)
    {
        if (data.HasValue("offset"))
            ok &= ReadLegacyValue<glm::vec2>("offset", "kWeight", data);
        if (data.HasValue("kWeight"))
            ok &= ReadLegacyValue<glm::vec2>("kWeight", "kGradientWeight", data);
    }
    else if (mType == Type::Texture)
    {
        if (data.HasValue("color"))
            ok &= ReadLegacyValue<Color4f>("color", "kBaseColor", data);

        if (!data.HasArray("texture_maps"))
        {
            if (data.HasChunk("texture_map"))
            {
                const auto& chunk = data.GetReadChunk("texture_map");
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromJson(*chunk);
            }
            else
            {
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromLegacyJsonTexture2D(data);
            }
            if (mTextureMaps.size() > 1)
                mTextureMaps.resize(1);
        }
    }
    else if (mType == Type::Sprite)
    {
        if (data.HasValue("color"))
            ok &= ReadLegacyValue<Color4f>("color", "kBaseColor", data);
        if (data.HasValue("blending"))
        {
            bool blend_frames = false;
            ok &= data.Read("blending", &blend_frames);
            mFlags.set(Flags::BlendFrames, blend_frames);
        }
        else if (data.HasValue("blend_frames"))
        {
            bool blend_frames = false;
            ok &= data.Read("blend_frames", &blend_frames);
            mFlags.set(Flags::BlendFrames, blend_frames);
        }

        if (!data.HasArray("texture_maps"))
        {
            if (data.GetNumChunks("sprites"))
            {
                const auto& chunk = data.GetReadChunk("sprites", 0);
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromJson(*chunk);
            }
            else
            {
                mTextureMaps.emplace_back(new TextureMap);
                ok &= mTextureMaps[0]->FromJson(data);
            }
        }
    }

    for (unsigned i=0; i<data.GetNumChunks("uniforms"); ++i)
    {
        Uniform uniform;
        const auto& chunk = data.GetReadChunk("uniforms", i);
        std::string name;
        ok &= chunk->Read("name", &name);
        ok &= chunk->Read("value", &uniform);
        mUniforms[std::move(name)] = std::move(uniform);
    }

    // this migration here is duplicate (there's also a migration in the
    // editor resource system) exists to simplify the preset particle
    // migration since those don't use the editor's resource system.
    if (base::Contains(mUniforms, "kParticleStartColor") &&
        base::Contains(mUniforms, "kParticleEndColor") &&
        !base::Contains(mUniforms, "kParticleMidColor"))
    {
        const auto& start_color = GetParticleStartColor();
        const auto& end_color = GetParticleEndColor();
        const auto& mid_color = start_color * 0.5f + end_color * 0.5f;
        SetParticleMidColor(mid_color);
        DEBUG("Fabricated particle material mid-way color value. [name='%1']", mName);
    }

    for (unsigned i=0; i<data.GetNumChunks("texture_maps"); ++i)
    {
        const auto& chunk = data.GetReadChunk("texture_maps", i);
        std::string name;
        TextureMap::Type type;
        if (chunk->Read("type", &type) && chunk->Read("name", &name))
        {
            auto map = std::make_unique<TextureMap>();

            if (type == TextureMap::Type::Texture2D)
            {
                if (!chunk->HasValue("sampler_name0"))
                    ok &= map->FromLegacyJsonTexture2D(*chunk);
                else ok &= map->FromJson(*chunk);
            }
            else
            {
                ok &= map->FromJson(*chunk);
            }
            mTextureMaps.push_back(std::move(map));
        } else ok = false;
    }

    if (mActiveTextureMap.empty())
    {
        if (!mTextureMaps.empty())
            mActiveTextureMap = mTextureMaps[0]->GetId();
    }

    const auto enable_cache = flags & LoadingFlags::EnableCaching;
    if (enable_cache)
    {
        ValueCache values;
        values.shader_hash = GetShaderHash();
        mCache = values;
    }

    return ok;
}

std::unique_ptr<MaterialClass> MaterialClass::Copy() const
{
    return std::make_unique<MaterialClass>(*this, true);
}

std::unique_ptr<MaterialClass> MaterialClass::Clone() const
{
    return std::make_unique<MaterialClass>(*this, false);
}

void MaterialClass::BeginPacking(TexturePacker* packer) const
{
    for (const auto& map : mTextureMaps)
    {
        for (size_t i=0; i<map->GetNumTextures(); ++i)
        {
            const auto& rect   = map->GetTextureRect(i);
            const auto* source = map->GetTextureSource(i);
            const TexturePacker::ObjectHandle handle = source;

            source->BeginPacking(packer);
            packer->SetTextureBox(handle, rect);

            // when texture rects are used to address a sub rect within the
            // texture wrapping on texture coordinates must be done "manually"
            // since the HW sampler coords are outside the sub rectangle coords.
            // for example if the wrapping is set to wrap on x and our box is
            // 0.25 units the HW sampler would not help us here to wrap when the
            // X coordinate is 0.26. Instead, we need to do the wrap manually.
            // However, this can cause rendering artifacts when texture sampling
            // is done depending on the current filter being used.
            bool can_combine = true;

            const auto& box = rect;
            const auto x = box.GetX();
            const auto y = box.GetY();
            const auto w = box.GetWidth();
            const auto h = box.GetHeight();
            const float eps = 0.001;
            // if the texture uses sub rect then we still have this problem and packing
            // won't make it any worse. in other words if the box is the normal 0.f - 1.0f
            // (meaning whole texture, not some part of it) then combining and using a
            // sub-rect can make the result worse if coordinate wrapping is in fact needed.
            if (math::equals(0.0f, x, eps) &&
                math::equals(0.0f, y, eps) &&
                math::equals(1.0f, w, eps) &&
                math::equals(1.0f, h, eps))
            {
                // is it possible for a texture to go beyond its range and require wrapping?
                // the only case we know here is when texture velocity is non-zero
                // or when texture scaling is used. we consider these properties to be
                // static and not be changed by the game by default at runtime.

                // velocity check
                const auto& velocity = GetTextureVelocity();
                const bool has_x_velocity = !math::equals(0.0f, velocity.x, eps);
                const bool has_y_velocity = !math::equals(0.0f, velocity.y, eps);
                if (has_x_velocity && (mTextureWrapX == TextureWrapping::Repeat ||
                                       mTextureWrapX == TextureWrapping::Mirror))
                    can_combine = false;
                else if (has_y_velocity && (mTextureWrapY == TextureWrapping::Repeat ||
                                            mTextureWrapY == TextureWrapping::Mirror))
                    can_combine = false;

                // scale check
                const auto& scale = GetTextureScale();
                if (scale.x > 1.0f && (mTextureWrapX == TextureWrapping::Repeat ||
                                       mTextureWrapX == TextureWrapping::Mirror))
                    can_combine = false;
                else if (scale.y > 1.0f && (mTextureWrapY == TextureWrapping::Repeat ||
                                            mTextureWrapY == TextureWrapping::Mirror))
                    can_combine = false;
            }
            packer->SetTextureFlag(handle, TexturePacker::TextureFlags::CanCombine, can_combine);

            if (mType == Type::Tilemap)
            {
                // since we're using absolute sizes for the tile specification the
                // texture cannot change or then the absolute dimensions must also change
                // (kTileOffset, kTileSize, kTilePadding).
                // Or then we disable the texture resizing for now.
                packer->SetTextureFlag(handle, TexturePacker::TextureFlags::AllowedToResize, false);
            }
        }
    }
}

void MaterialClass::FinishPacking(const TexturePacker* packer)
{
    for (auto& map : mTextureMaps)
    {
        for (size_t i=0; i<map->GetNumTextures(); ++i)
        {
            auto* source = map->GetTextureSource(i);
            const TexturePacker::ObjectHandle handle = source;
            source->FinishPacking(packer);
            map->SetTextureRect(i, packer->GetPackedTextureBox(handle));
        }
    }
}

unsigned MaterialClass::FindTextureMapIndexById(const std::string& id) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextureMaps(); ++i)
    {
        const auto* map = GetTextureMap(i);
        if (map->GetId() == id)
            break;
    }
    return i;
}

unsigned MaterialClass::FindTextureMapIndexBySampler(const std::string& name, unsigned sampler_index) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextureMaps(); ++i)
    {
        const auto* map = GetTextureMap(i);
        if (map->GetSamplerName(sampler_index) == name)
            break;
    }
    return i;
}

unsigned MaterialClass::FindTextureMapIndexByName(const std::string& name) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextureMaps(); ++i)
    {
        const auto* map = GetTextureMap(i);
        if (map->GetName() == name)
            break;
    }
    return i;
}

TextureMap* MaterialClass::FindTextureMapBySampler(const std::string& name, unsigned sampler_index)
{
    const auto index = FindTextureMapIndexBySampler(name, sampler_index);
    if (index == GetNumTextureMaps())
        return nullptr;

    return GetTextureMap(index);
}

TextureMap* MaterialClass::FindTextureMapByName(const std::string& name)
{
    const auto index = FindTextureMapIndexByName(name);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}
TextureMap* MaterialClass::FindTextureMapById(const std::string& id)
{
    const auto index = FindTextureMapIndexById(id);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}

const TextureMap* MaterialClass::FindTextureMapBySampler(const std::string& name, unsigned sampler_index) const
{
    const auto index = FindTextureMapIndexBySampler(name, sampler_index);
    if (index == GetNumTextureMaps())
        return nullptr;

    return GetTextureMap(index);
}


const TextureMap* MaterialClass::FindTextureMapByName(const std::string& name) const
{
    const auto index = FindTextureMapIndexByName(name);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}
const TextureMap* MaterialClass::FindTextureMapById(const std::string& id) const
{
    const auto index = FindTextureMapIndexById(id);
    if (index == GetNumTextureMaps())
        return nullptr;
    return GetTextureMap(index);
}

void MaterialClass::SetTexture(std::unique_ptr<TextureSource> source)
{
    if (mTextureMaps.size() != 1)
        mTextureMaps.resize(1);
    if (mTextureMaps[0] == nullptr)
    {
        mTextureMaps[0] = std::make_unique<TextureMap>();
        if (mType == Type::Sprite)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Sprite);
            mTextureMaps[0]->SetName("Sprite");
        }
        else if (mType == Type::Texture)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Texture2D);
            mTextureMaps[0]->SetName("Texture");
        }
    }
    mTextureMaps[0]->SetNumTextures(1);
    mTextureMaps[0]->SetTextureSource(0, std::move(source));
}

void MaterialClass::AddTexture(std::unique_ptr<TextureSource> source)
{
    if (mTextureMaps.size() != 1)
        mTextureMaps.resize(1);
    if (mTextureMaps[0] == nullptr)
    {
        mTextureMaps[0] = std::make_unique<TextureMap>();
        if (mType == Type::Sprite)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Sprite);
            mTextureMaps[0]->SetName("Sprite");
        }
        else if (mType == Type::Texture)
        {
            mTextureMaps[0]->SetType(TextureMap::Type::Texture2D);
            mTextureMaps[0]->SetName("Texture");
        }
    }
    const auto count = mTextureMaps[0]->GetNumTextures();
    mTextureMaps[0]->SetNumTextures(count+1);
    mTextureMaps[0]->SetTextureSource(count, std::move(source));
}

void MaterialClass::DeleteTextureMap(const std::string& id) noexcept
{
    base::EraseRemove(mTextureMaps, [&id](const auto& map) {
        return map->GetId() == id;
    });
}
void MaterialClass::DeleteTextureSrc(const std::string& id) noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        map->DeleteTexture(index);
    }
}

TextureSource* MaterialClass::FindTextureSource(const std::string& id) noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        return map->GetTextureSource(index);
    }
    return nullptr;
}

const TextureSource* MaterialClass::FindTextureSource(const std::string& id) const noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        return map->GetTextureSource(index);
    }
    return nullptr;
}

FRect MaterialClass::FindTextureRect(const std::string& id) const noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        return map->GetTextureRect(index);
    }
    return {};
}

void MaterialClass::SetTextureRect(const std::string& id, const gfx::FRect& rect) noexcept
{
    for (auto& map : mTextureMaps)
    {
        auto index = map->FindTextureSourceIndexById(id);
        if (index == map->GetNumTextures())
            continue;
        map->SetTextureRect(index, rect);
        return;
    }
}

void MaterialClass::SetTextureRect(size_t map, size_t texture, const gfx::FRect& rect) noexcept
{
    base::SafeIndex(mTextureMaps, map)->SetTextureRect(texture, rect);
}

void MaterialClass::SetTextureRect(const gfx::FRect& rect) noexcept
{
    SetTextureRect(0, 0, rect);
}

void MaterialClass::SetTextureSource(size_t map, size_t texture, std::unique_ptr<TextureSource> source) noexcept
{
    base::SafeIndex(mTextureMaps, map)->SetTextureSource(texture, std::move(source));
}

void MaterialClass::SetTextureSource(std::unique_ptr<TextureSource> source) noexcept
{
    SetTextureSource(0, 0, std::move(source));
}

// static
std::string MaterialClass::GetColorUniformName(ColorIndex index)
{
    if (index == ColorIndex::BaseColor)
        return "kBaseColor";
    else if (index == ColorIndex::GradientColor0)
        return "kGradientColor0";
    else if (index == ColorIndex::GradientColor1)
        return "kGradientColor1";
    else if (index == ColorIndex::GradientColor2)
        return "kGradientColor2";
    else if (index == ColorIndex::GradientColor3)
        return "kGradientColor3";
    else if (index == ColorIndex::AmbientColor)
        return "kAmbientColor";
    else if (index == ColorIndex::DiffuseColor)
        return "kDiffuseColor";
    else if (index == ColorIndex::SpecularColor)
        return "kSpecularColor";
    else if (index == ColorIndex::ParticleStartColor)
        return "kParticleStartColor";
    else if (index == ColorIndex::ParticleEndColor)
        return "kParticleEndColor";
    else if (index == ColorIndex::ParticleMidColor)
        return "kParticleMidColor";
    else BUG("Unknown color index.");
    return "";
}

// static
std::unique_ptr<MaterialClass> MaterialClass::ClassFromJson(const data::Reader& data, unsigned flags)
{
    Type type;
    if (!data.Read("type", &type))
        return nullptr;

    auto klass = std::make_unique<MaterialClass>(type);

    // todo: change the API so that we can also return the OK value.
    bool ok = klass->FromJson(data);
    return klass;
}

MaterialClass& MaterialClass::operator=(const MaterialClass& other)
{
    if (this == &other)
        return *this;

    MaterialClass tmp(other, true);
    std::swap(mClassId         , tmp.mClassId);
    std::swap(mName            , tmp.mName);
    std::swap(mType            , tmp.mType);
    std::swap(mFlags           , tmp.mFlags);
    std::swap(mShaderUri       , tmp.mShaderUri);
    std::swap(mShaderSrc       , tmp.mShaderSrc);
    std::swap(mActiveTextureMap, tmp.mActiveTextureMap);
    std::swap(mSurfaceType     , tmp.mSurfaceType);
    std::swap(mTextureMinFilter, tmp.mTextureMinFilter);
    std::swap(mTextureMagFilter, tmp.mTextureMagFilter);
    std::swap(mTextureWrapX    , tmp.mTextureWrapX);
    std::swap(mTextureWrapY    , tmp.mTextureWrapY);
    std::swap(mUniforms        , tmp.mUniforms);
    std::swap(mTextureMaps     , tmp.mTextureMaps);
    return *this;
}

TextureMap* MaterialClass::SelectTextureMap(const State& state) const noexcept
{
    if (mTextureMaps.empty())
    {
        if (state.first_render)
            WARN("Material has no texture maps. [name='%1'", mName);
        return nullptr;
    }

    if (!state.active_texture_map_id.empty())
    {
        for (auto& map : mTextureMaps)
        {
            if (map->GetId() == state.active_texture_map_id)
                return map.get();
        }
        if (state.first_render)
            WARN("No such texture map found in material. Falling back on default. [name='%1', map=%2]", mName,
                 state.active_texture_map_id);
    }

    // keep previous semantics, so default to the first map for the
    // material and sprite maps.
    if (mActiveTextureMap.empty())
        return mTextureMaps[0].get();

    for (auto& map : mTextureMaps)
    {
        if (map->GetId() == mActiveTextureMap)
            return map.get();
    }
    if (state.first_render)
        WARN("No such texture map found in material. Using first map. [name='%1', map=%2]", mName, mActiveTextureMap);

    return mTextureMaps[0].get();
}

ShaderSource MaterialClass::GetShaderSource(const State& state, const Device& device) const
{
    if (mType == Type::Custom)
    {
        if (!mShaderSrc.empty())
            return ShaderSource::FromRawSource(mShaderSrc, ShaderSource::Type::Fragment);

        if (mShaderUri.empty())
        {
            ERROR("Material has no shader source specified. [name='%1']", mName);
            return {};
        }

        Loader::ResourceDesc desc;
        desc.uri = mShaderUri;
        desc.id = mClassId;
        desc.type = Loader::Type::Shader;
        const auto& buffer = gfx::LoadResource(desc);
        if (!buffer)
        {
            ERROR("Failed to load custom material shader source file. [name='%1', uri='%2']", mName, mShaderUri);
            return {};
        }
        const char* beg = (const char*) buffer->GetData();
        const char* end = beg + buffer->GetByteSize();
        DEBUG("Loading custom shader source. [uri=%1', material='%2']", mShaderUri, mName);

        ShaderSource source;
        source.SetType(ShaderSource::Type::Fragment);
        source.LoadRawSource(std::string(beg, end));
        source.AddShaderSourceUri(mShaderUri);
        source.AddShaderName(mName);
        return source;
    }


    static const char* texture_functions =  {
#include "shaders/fragment_texture_functions.glsl"
    };
    static const char* base_shader = {
#include "shaders/fragment_shader_base.glsl"
    };

    ShaderSource src;
    src.SetType(gfx::ShaderSource::Type::Fragment);
    if (IsStatic() || !mShaderSrc.empty())
    {
        src.AddDebugInfo("Shader", base::FormatString("%1 Shader", mType));
        src.AddDebugInfo("Material Name", mName);
        src.AddDebugInfo("Material ID", mClassId);
    }
    else
    {
        src.AddDebugInfo("Shader", base::FormatString("%1 Shader", mType));
    }

    if (mType == Type::Color)
    {
        static const char* source = {
#include "shaders/fragment_color_shader.glsl"
        };

        src.LoadRawSource(base_shader);
        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_shader_base.glsl");
        src.AddShaderSourceUri("shaders/fragment_color_shader.glsl");
    }
    else if (mType == Type::Gradient)
    {
        static const char* source = {
#include "shaders/fragment_gradient_shader.glsl"
        };

        src.LoadRawSource(base_shader);
        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_shader_base.glsl");
        src.AddShaderSourceUri("shaders/fragment_gradient_shader.glsl");
    }
    else if (mType == Type::Sprite)
    {
        static const char* source = {
#include "shaders/fragment_sprite_shader.glsl"
        };

        src.LoadRawSource(base_shader);
        src.LoadRawSource(texture_functions);
        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_shader_base.glsl");
        src.AddShaderSourceUri("shaders/fragment_texture_functions.glsl");
        src.AddShaderSourceUri("shaders/fragment_sprite_shader.glsl");
    }
    else if (mType == Type::Texture)
    {
        static const char* source =  {
#include "shaders/fragment_texture_shader.glsl"
        };

        src.LoadRawSource(base_shader);
        src.LoadRawSource(texture_functions);
        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_shader_base.glsl");
        src.AddShaderSourceUri("shaders/fragment_texture_functions.glsl");
        src.AddShaderSourceUri("shaders/fragment_texture_shader.glsl");
    }
    else if (mType == Type::Tilemap)
    {
        static const char* source = {
#include "shaders/fragment_tilemap_shader.glsl"
        };

        src.LoadRawSource(base_shader);
        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_shader_base.glsl");
        src.AddShaderSourceUri("shaders/fragment_tilemap_shader.glsl");
    }
    else if (mType == Type::Particle2D)
    {
        static const char* source = {
#include "shaders/fragment_2d_particle_shader.glsl"
        };

        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_2d_particle_shader.glsl");
    }
    else if (mType == Type::BasicLight)
    {
        static const char* source = {
#include "shaders/fragment_basic_light_material_shader.glsl"
        };
        src.LoadRawSource(base_shader);
        src.LoadRawSource(source);
        src.AddShaderSourceUri("shaders/fragment_shader_base.glsl");
        src.AddShaderSourceUri("shaders/fragment_basic_light_material_shader.glsl");
    }
    else BUG("Unknown material type.");

    if (!mShaderSrc.empty())
    {
        src.AddPreprocessorDefinition("CUSTOM_FRAGMENT_MAIN");
        src.ReplaceToken("CUSTOM_FRAGMENT_MAIN", mShaderSrc);
    }
    return src;
}

bool MaterialClass::ApplySpriteDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    auto* map = SelectTextureMap(state);
    if (map == nullptr)
    {
        if (state.first_render)
            WARN("Failed to select texture map. [material='%1']", mName);
        return false;
    }

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !IsStatic();
    ts.current_time    = state.material_time;
    ts.group_tag       = mClassId;

    TextureMap::BoundState binds;
    if (!map->BindTextures(ts, device,  binds))
    {
        if (state.first_render)
            ERROR("Failed to bind sprite textures. [material='%1']", mName);
        return false;
    }

    glm::vec2 alpha_mask;

    bool need_software_wrap = true;
    for (unsigned i=0; i<2; ++i)
    {
        auto* texture = binds.textures[i];
        // set texture properties *before* setting it to the program.
        texture->SetFilter(mTextureMinFilter);
        texture->SetFilter(mTextureMagFilter);
        texture->SetWrapX(mTextureWrapX);
        texture->SetWrapY(mTextureWrapY);
        texture->SetGroup(mClassId);

        alpha_mask[i] = texture->IsAlphaMask() ? 1.0f : 0.0f;

        const auto& box = binds.rects[i];
        const float x  = box.GetX();
        const float y  = box.GetY();
        const float sx = box.GetWidth();
        const float sy = box.GetHeight();
        const auto& kTextureName = "kTexture" + std::to_string(i);
        const auto& kTextureRect = "kTextureBox" + std::to_string(i);
        program.SetTexture(kTextureName.c_str(), i, *texture);
        program.SetUniform(kTextureRect.c_str(), x, y, sx, sy);

        // if a sub-rectangle is defined we need to use software (shader)
        // based wrapping/clamping in order to wrap/clamp properly within
        // the bounds of the sub rect.
        // we do this check here rather than introduce a specific flag for this purpose.
        const float eps = 0.001;
        if (math::equals(0.0f, x, eps) &&
            math::equals(0.0f, y, eps) &&
            math::equals(1.0f, sx, eps) &&
            math::equals(1.0f, sy, eps))
            need_software_wrap = false;
    }

    const float kBlendCoeff  = BlendFrames() ? binds.blend_coefficient : 0.0f;
    program.SetTextureCount(2);
    program.SetUniform("kBlendCoeff", kBlendCoeff);
    program.SetUniform("kAlphaMask", alpha_mask);

    if (state.draw_category == DrawCategory::Particles)
    {
        const auto effect = static_cast<int>(GetParticleEffect());
        program.SetUniform("kParticleEffect", effect);
    }

    // set software wrap/clamp. -1 = disabled.
    if (need_software_wrap)
    {
        program.SetUniform("kTextureWrap", static_cast<int>(mTextureWrapX), static_cast<int>(mTextureWrapY));
    }
    else
    {
        program.SetUniform("kTextureWrap", -1, -1);
    }
    if (!IsStatic())
    {
        SetUniform("kBaseColor",         state.uniforms, GetBaseColor(), program);
        SetUniform("kTextureScale",      state.uniforms, GetTextureScale(), program);
        SetUniform("kTextureVelocity",   state.uniforms, GetTextureVelocity(), program);
        SetUniform("kTextureRotation",   state.uniforms, GetTextureRotation(), program);
        SetUniform("kAlphaCutoff",       state.uniforms, GetAlphaCutoff(), program);
    }
    return true;
}


bool MaterialClass::ApplyTextureDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    auto* map = SelectTextureMap(state);
    if (map == nullptr)
    {
        if (state.first_render)
            ERROR("Failed to select material texture map. [material='%1']", mName);
        return false;
    }

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !IsStatic();
    ts.current_time    = 0.0;

    TextureMap::BoundState binds;
    if (!map->BindTextures(ts, device, binds))
    {
        if (state.first_render)
            ERROR("Failed to bind material texture. [material='%1']", mName);
        return false;
    }

    auto* texture = binds.textures[0];
    texture->SetFilter(mTextureMinFilter);
    texture->SetFilter(mTextureMagFilter);
    texture->SetWrapX(mTextureWrapX);
    texture->SetWrapY(mTextureWrapY);

    const auto rect = binds.rects[0];
    const float x  = rect.GetX();
    const float y  = rect.GetY();
    const float sx = rect.GetWidth();
    const float sy = rect.GetHeight();

    bool need_software_wrap = true;
    // if a sub-rectangle is defined we need to use software (shader)
    // based wrapping/clamping in order to wrap/clamp properly within
    // the bounds of the sub rect.
    // we do this check here rather than introduce a specific flag for this purpose.
    const float eps = 0.001;
    if (math::equals(0.0f, x, eps) &&
        math::equals(0.0f, y, eps) &&
        math::equals(1.0f, sx, eps) &&
        math::equals(1.0f, sy, eps))
        need_software_wrap = false;

    program.SetTextureCount(1);
    program.SetTexture("kTexture", 0, *texture);
    program.SetUniform("kTextureBox", x, y, sx, sy);
    program.SetUniform("kAlphaMask", binds.textures[0]->IsAlphaMask() ? 1.0f : 0.0f);

    if (state.draw_category == DrawCategory::Particles)
    {
        const auto effect = static_cast<int>(GetParticleEffect());
        program.SetUniform("kParticleEffect", effect);
    }

    // set software wrap/clamp. -1 = disabled.
    if (need_software_wrap)
    {
        program.SetUniform("kTextureWrap", static_cast<int>(mTextureWrapX), static_cast<int>(mTextureWrapY));
    }
    else
    {
        program.SetUniform("kTextureWrap", -1, -1);
    }
    if (!IsStatic())
    {
        SetUniform("kBaseColor",         state.uniforms, GetBaseColor(), program);
        SetUniform("kTextureScale",      state.uniforms, GetTextureScale(), program);
        SetUniform("kTextureVelocity",   state.uniforms, GetTextureVelocity(), program);
        SetUniform("kTextureRotation",   state.uniforms, GetTextureRotation(), program);
        SetUniform("kAlphaCutoff",       state.uniforms, GetAlphaCutoff(), program);
    }
    return true;
}

bool MaterialClass::ApplyTilemapDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    auto* map = SelectTextureMap(state);
    if (map == nullptr)
    {
        if (state.first_render)
            ERROR("Failed to select material texture map. [material='%1']", mName);
        return false;
    }

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !IsStatic();
    ts.current_time    = 0.0;

    TextureMap::BoundState binds;
    if (!map->BindTextures(ts, device, binds))
    {
        if (state.first_render)
            ERROR("Failed to bind material texture. [material='%1']", mName);
        return false;
    }

    auto* texture = binds.textures[0];
    texture->SetFilter(mTextureMinFilter);
    texture->SetFilter(mTextureMagFilter);
    texture->SetWrapX(mTextureWrapX);
    texture->SetWrapY(mTextureWrapY);

    const auto rect = binds.rects[0];
    const float x  = rect.GetX();
    const float y  = rect.GetY();
    const float sx = rect.GetWidth();
    const float sy = rect.GetHeight();

    const float width  = texture->GetWidth();
    const float height = texture->GetHeight();

    program.SetUniform("kTextureSize", glm::vec2{width, height});
    program.SetTexture("kTexture", 0, *texture);
    program.SetUniform("kTextureBox", x, y, sx, sy);
    program.SetTextureCount(1);

    if (!IsStatic())
    {
        SetUniform("kBaseColor",         state.uniforms, GetBaseColor(), program);
        SetUniform("kAlphaCutoff",       state.uniforms, GetAlphaCutoff(), program);
        SetUniform("kTileSize",          state.uniforms, GetTileSize(), program);
        SetUniform("kTileOffset",        state.uniforms, GetTileOffset(), program);
        SetUniform("kTilePadding",       state.uniforms, GetTilePadding(), program);
    }

    if (state.draw_category == DrawCategory::Basic)
        SetUniform("kTileIndex", state.uniforms, GetUniformValue("kTileIndex", 0.0f), program);
    return true;
}

bool MaterialClass::ApplyParticleDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    auto* texture_map = SelectTextureMap(state);
    if (texture_map == nullptr)
    {
        if (state.first_render)
            ERROR("Failed to select material texture map. [material='%1']", mName);
        return false;
    }

    TextureMap::BindingState ts;
    ts.dynamic_content = state.editing_mode || !IsStatic();
    ts.current_time    = 0.0;

    TextureMap::BoundState binds;
    if (!texture_map->BindTextures(ts, device, binds))
    {
        if (state.first_render)
            ERROR("Failed to bind material texture. [material='%1]", mName);
        return false;
    }
    auto* texture = binds.textures[0];
    texture->SetFilter(mTextureMinFilter);
    texture->SetFilter(mTextureMagFilter);
    texture->SetWrapX(mTextureWrapX);
    texture->SetWrapY(mTextureWrapY);

    const auto rect = binds.rects[0];
    const float x  = rect.GetX();
    const float y  = rect.GetY();
    const float sx = rect.GetWidth();
    const float sy = rect.GetHeight();

    program.SetTexture("kMask", 0, *texture);
    program.SetUniform("kMaskRect", x, y, sx, sy);
    program.SetTextureCount(1);

    if (!IsStatic())
    {
        SetUniform("kParticleStartColor",   state.uniforms, GetParticleStartColor(), program);
        SetUniform("kParticleEndColor",     state.uniforms, GetParticleEndColor(), program);
        SetUniform("kParticleMidColor",     state.uniforms, GetParticleMidColor(), program);
        SetUniform("kParticleBaseRotation", state.uniforms, GetParticleBaseRotation(), program);
    }
    SetUniform("kParticleRotation", state.uniforms, static_cast<unsigned>(GetParticleRotation()), program);

    return true;
}

bool MaterialClass::ApplyBasicLightDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    if (!IsStatic())
    {
        SetUniform("kAmbientColor",     state.uniforms, GetAmbientColor(), program);
        SetUniform("kDiffuseColor",     state.uniforms, GetDiffuseColor(), program);
        SetUniform("kSpecularColor",    state.uniforms, GetSpecularColor(), program);
        SetUniform("kSpecularExponent", state.uniforms, GetSpecularExponent(), program);
    }

    struct Map {
        const char* texture_map_name;
        const char* texture_rect_name;
        BasicLightMaterialMap type;
    } maps[] = {
        { "kDiffuseMap",  "kDiffuseMapRect",  BasicLightMaterialMap::Diffuse },
        { "kSpecularMap", "kSpecularMapRect", BasicLightMaterialMap::Specular },
        { "kNormalMap",   "kNormalMapRect",   BasicLightMaterialMap::Normal }
    };
    unsigned map_flags = 0;

    for (unsigned i=0; i<base::ArraySize(maps); ++i)
    {
        // these textures are optional so if there's no map or the map doesn't
        // have any textures set then we're just going to skip binding it.
        const auto* texture_map = FindTextureMapBySampler(maps[i].texture_map_name, 0);
        if (!texture_map || texture_map->GetNumTextures() == 0)
            continue;

        map_flags |= static_cast<unsigned>(maps[i].type);

        TextureMap::BindingState ts;
        ts.dynamic_content = state.editing_mode || !IsStatic();
        ts.current_time = state.material_time;

        TextureMap::BoundState binds;
        if (!texture_map->BindTextures(ts, device, binds))
        {
            if (state.first_render)
                ERROR("Failed to bind basic light material map. [material='%1', map=%2]", mName, maps[i].type);
            return false;
        }
        auto* texture = binds.textures[0];
        texture->SetFilter(mTextureMinFilter);
        texture->SetFilter(mTextureMagFilter);
        texture->SetWrapX(mTextureWrapX);
        texture->SetWrapY(mTextureWrapY);

        const auto rect = binds.rects[0];
        const float x  = rect.GetX();
        const float y  = rect.GetY();
        const float sx = rect.GetWidth();
        const float sy = rect.GetHeight();

        program.SetTexture(maps[i].texture_map_name, i, *texture);
        program.SetUniform(maps[i].texture_rect_name, x, y, sx, sy);
    }
    program.SetUniform("kMaterialMaps", map_flags);

    return true;
}

bool MaterialClass::ApplyCustomDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    for (const auto& uniform : mUniforms)
    {
        const char* name = uniform.first.c_str();
        if (const auto* ptr = std::get_if<float>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<int>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<glm::vec2>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<glm::vec3>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<glm::vec4>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<Color4f>(&uniform.second))
            MaterialClass::SetUniform(name, state.uniforms, *ptr, program);
        else if (const auto* ptr = std::get_if<std::string>(&uniform.second)) {
            // ignored right now, no use for this type. we're (ab)using the uniforms
            // to change the active texture map in some other material types.
        } else BUG("Unhandled uniform type.");
    }

    unsigned texture_unit = 0;

    for (const auto& map : mTextureMaps)
    {
        TextureMap::BindingState ts;
        ts.dynamic_content = true; // todo: need static flag. for now use dynamic (which is slower) but always correct
        ts.current_time    = state.material_time;
        ts.group_tag       = mClassId;
        TextureMap::BoundState binds;
        if (!map->BindTextures(ts, device, binds))
            return false;
        for (unsigned i=0; i<2; ++i)
        {
            auto* texture = binds.textures[i];
            if (texture == nullptr)
                continue;

            texture->SetFilter(mTextureMinFilter);
            texture->SetFilter(mTextureMagFilter);
            texture->SetWrapX(mTextureWrapX);
            texture->SetWrapY(mTextureWrapY);
            texture->SetGroup(mClassId);

            const auto& rect = binds.rects[i];
            if (!binds.sampler_names[i].empty())
                program.SetTexture(binds.sampler_names[i].c_str(), texture_unit, *texture);
            if (!binds.rect_names[i].empty())
                program.SetUniform(binds.rect_names[i].c_str(),
                                   rect.GetX(), rect.GetY(),
                                   rect.GetWidth(), rect.GetHeight());
            texture_unit++;
        }
    }
    program.SetTextureCount(texture_unit);
    return true;
}

GradientClass CreateMaterialClassFromColor(const Color4f& top_left,
                                           const Color4f& top_right,
                                           const Color4f& bottom_left,
                                           const Color4f& bottom_right)
{
    MaterialClass material(MaterialClass::Type::Gradient, std::string(""));
    material.SetColor(top_left, GradientClass::ColorIndex::GradientColor0);
    material.SetColor(top_right, GradientClass::ColorIndex::GradientColor1);
    material.SetColor(bottom_left, GradientClass::ColorIndex::GradientColor2);
    material.SetColor(bottom_right, GradientClass::ColorIndex::GradientColor3);
    return material;
}

ColorClass CreateMaterialClassFromColor(const Color4f& color)
{
    const auto alpha = color.Alpha();

    MaterialClass material(MaterialClass::Type::Color, std::string(""));
    material.SetBaseColor(color);
    material.SetSurfaceType(alpha == 1.0f
        ? MaterialClass::SurfaceType::Opaque
        : MaterialClass::SurfaceType::Transparent);
    return material;
}

MaterialClass CreateMaterialClassFromSprite(const std::string& uri)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetName("Sprite");
    map->SetNumTextures(1);
    map->SetTextureSource(0, LoadTextureFromFile(uri, ""));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

TextureMap2DClass CreateMaterialClassFromImage(const std::string& uri)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetName("Sprite");
    map->SetNumTextures(1);
    map->SetTextureSource(0, LoadTextureFromFile(uri, ""));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Opaque);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

SpriteClass CreateMaterialClassFromImages(const std::initializer_list<std::string>& uris)
{
    const std::vector<std::string> tmp(uris);

    auto map = std::make_unique<TextureMap>("");
    map->SetName("Sprite");
    map->SetType(TextureMap::Type::Sprite);
    map->SetNumTextures(uris.size());
    for (size_t i=0; i<uris.size(); ++i)
        map->SetTextureSource(i, LoadTextureFromFile(tmp[i], ""));

    MaterialClass material(MaterialClass::Type::Sprite, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

SpriteClass CreateMaterialClassFromImages(const std::vector<std::string>& uris)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetName("Sprite");
    map->SetType(TextureMap::Type::Sprite);
    map->SetNumTextures(uris.size());
    for (size_t i=0; i<uris.size(); ++i)
        map->SetTextureSource(i, LoadTextureFromFile(uris[i], ""));

    SpriteClass material(MaterialClass::Type::Sprite, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

SpriteClass CreateMaterialClassFromSpriteAtlas(const std::string& uri, const std::vector<FRect>& frames)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetName("Sprite");
    map->SetType(TextureMap::Type::Sprite);
    map->SetNumTextures(uri.size());
    for (size_t i=0; i<frames.size(); ++i)
    {
        map->SetTextureSource(i, LoadTextureFromFile(uri, ""));
        map->SetTextureRect(i, frames[i]);
    }

    MaterialClass material(MaterialClass::Type::Sprite, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

TextureMap2DClass CreateMaterialClassFromText(const TextBuffer& text)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetType(TextureMap::Type::Texture2D);
    map->SetName("Text");
    map->SetNumTextures(1);
    map->SetTextureSource(0, CreateTextureFromText(text, ""));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

TextureMap2DClass CreateMaterialClassFromText(TextBuffer&& text)
{
    auto map = std::make_unique<TextureMap>("");
    map->SetType(TextureMap::Type::Texture2D);
    map->SetName("Text");
    map->SetNumTextures(1);
    map->SetTextureSource(0, CreateTextureFromText(std::move(text), ""));

    MaterialClass material(MaterialClass::Type::Texture, std::string(""));
    material.SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    material.SetNumTextureMaps(1);
    material.SetTextureMap(0, std::move(map));
    return material;
}

} // namespace

