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
#include "graphics/shadersource.h"
#include "graphics/material_class.h"
#include "graphics/texture_source.h"
#include "graphics/texture_texture_source.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_bitmap_buffer_source.h"
#include "graphics/texture_bitmap_generator_source.h"
#include "graphics/texture_text_buffer_source.h"
#include "graphics/packer.h"
#include "graphics/loader.h"

namespace gfx
{

MaterialClass::MaterialClass(Type type, std::string id)
  : mClassId(std::move(id))
  , mType(type)
{
    mFlags.set(Flags::BlendFrames, true);
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
    mParticleAction   = other.mParticleAction;
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

MaterialClass::~MaterialClass() = default;

std::string MaterialClass::GetShaderName(const State& state) const noexcept
{
    if (mType == Type::Custom)
        return mName;
    if (IsStatic())
        return mName;
    return base::FormatString("BuiltIn%1Shader", mType);
}

std::string MaterialClass::GetShaderId(const State& state) const noexcept
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
            hash = base::hash_combine(hash, GetColor(ColorIndex::TopLeft));
            hash = base::hash_combine(hash, GetColor(ColorIndex::TopRight));
            hash = base::hash_combine(hash, GetColor(ColorIndex::BottomLeft));
            hash = base::hash_combine(hash, GetColor(ColorIndex::BottomRight));
            hash = base::hash_combine(hash, GetColorWeight());
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
        }
    }
    else if (mType == Type::Custom)
    {
        // todo: static uniform information

    } else BUG("Unknown material type.");

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
    hash = base::hash_combine(hash, mParticleAction);
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
    hash = base::hash_combine(hash, GetColor(ColorIndex::TopLeft));
    hash = base::hash_combine(hash, GetColor(ColorIndex::TopRight));
    hash = base::hash_combine(hash, GetColor(ColorIndex::BottomLeft));
    hash = base::hash_combine(hash, GetColor(ColorIndex::BottomRight));
    hash = base::hash_combine(hash, GetColorWeight());
    hash = base::hash_combine(hash, GetAlphaCutoff());
    hash = base::hash_combine(hash, GetTileSize());
    hash = base::hash_combine(hash, GetTileOffset());

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

    if (!source.HasDataDeclaration("PI", ShaderSource::ShaderDataDeclarationType::PreprocessorDefine))
        source.AddPreprocessorDefinition("PI", float(3.1415926));

    source.AddPreprocessorDefinition("MATERIAL_SURFACE_TYPE_OPAQUE", static_cast<int>(SurfaceType::Opaque));
    source.AddPreprocessorDefinition("MATERIAL_SURFACE_TYPE_TRANSPARENT", static_cast<int>(SurfaceType::Transparent));
    source.AddPreprocessorDefinition("MATERIAL_SURFACE_TYPE_EMISSIVE", static_cast<int>(SurfaceType::Emissive));
    source.AddPreprocessorDefinition("DRAW_PRIMITIVE_POINTS", static_cast<int>(DrawPrimitive::Points));
    source.AddPreprocessorDefinition("DRAW_PRIMITIVE_LINES", static_cast<int>(DrawPrimitive::Lines));
    source.AddPreprocessorDefinition("DRAW_PRIMITIVE_TRIANGLES", static_cast<int>(DrawPrimitive::Triangles));

    if (IsStatic())
    {
        // fold a set of known uniforms to constants in the shader
        // code so that we don't need to set them at runtime.
        // the tradeoff is that this creates more shader programs!
        source.FoldUniform("kAlphaCutoff", GetAlphaCutoff());
        source.FoldUniform("kBaseColor", GetColor(ColorIndex::BaseColor));
        source.FoldUniform("kColor0", GetColor(ColorIndex::TopLeft));
        source.FoldUniform("kColor1", GetColor(ColorIndex::TopRight));
        source.FoldUniform("kColor2", GetColor(ColorIndex::BottomLeft));
        source.FoldUniform("kColor3", GetColor(ColorIndex::BottomRight));
        source.FoldUniform("kOffset", GetColorWeight());
        source.FoldUniform("kTextureVelocity", GetTextureVelocity());
        source.FoldUniform("kTextureVelocityXY", glm::vec2(GetTextureVelocity()));
        source.FoldUniform("kTextureVelocityZ", GetTextureVelocity().z);
        source.FoldUniform("kTextureRotation", GetTextureRotation());
        source.FoldUniform("kTextureScale", GetTextureScale());
        source.FoldUniform("kTileSize", GetTileSize());
        source.FoldUniform("kTileOffset", GetTileOffset());
        source.FoldUniform("kTilePadding", GetTilePadding());
        source.FoldUniform("kSurfaceType", (int)mSurfaceType);
    }
    return source;
}

bool MaterialClass::ApplyDynamicState(const State& state, Device& device, ProgramState& program) const noexcept
{
    const bool render_points = state.draw_primitive == DrawPrimitive::Points;
    // todo: kill the kRenderPoints uniform
    program.SetUniform("kRenderPoints", render_points ? 1.0f : 0.0f);
    program.SetUniform("kTime", (float)state.material_time);
    program.SetUniform("kEditingMode", (int)state.editing_mode);
    // todo: fix this, point rendering could be used without particles.
    program.SetUniform("kParticleEffect", render_points ? (int)mParticleAction : 0);

    // for the future... for different render passes we got two options
    // either the single shader implements the different render pass
    // functionality or then there are different shaders for different passes
    // program.SetUniform("kRenderPass", (int)state.renderpass);

    program.SetUniform("kSurfaceType", static_cast<int>(mSurfaceType));
    program.SetUniform("kDrawPrimitive", static_cast<int>(state.draw_primitive));

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
            SetUniform("kColor0", state.uniforms, GetColor(ColorIndex::TopLeft),     program);
            SetUniform("kColor1", state.uniforms, GetColor(ColorIndex::TopRight),    program);
            SetUniform("kColor2", state.uniforms, GetColor(ColorIndex::BottomLeft),  program);
            SetUniform("kColor3", state.uniforms, GetColor(ColorIndex::BottomRight), program);
            SetUniform("kOffset", state.uniforms, GetColorWeight(), program);
        }
    }
    else if (mType == Type::Sprite)
        return ApplySpriteDynamicState(state, device, program);
    else if (mType == Type::Texture)
        return ApplyTextureDynamicState(state, device, program);
    else if (mType == Type::Tilemap)
        return ApplyTilemapDynamicState(state, device, program);
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
        program.SetUniform("kColor0", GetColor(ColorIndex::TopLeft));
        program.SetUniform("kColor1", GetColor(ColorIndex::TopRight));
        program.SetUniform("kColor2", GetColor(ColorIndex::BottomLeft));
        program.SetUniform("kColor3", GetColor(ColorIndex::BottomRight));
        program.SetUniform("kOffset", GetColorWeight());
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
    data.Write("particle_action",    mParticleAction);
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

bool MaterialClass::FromJson(const data::Reader& data)
{
    bool ok = true;
    ok &= data.Read("type",               &mType);
    ok &= data.Read("id",                 &mClassId);
    ok &= data.Read("name",               &mName);
    ok &= data.Read("shader_uri",         &mShaderUri);
    ok &= data.Read("shader_src",         &mShaderSrc);
    ok &= data.Read("active_texture_map", &mActiveTextureMap);
    ok &= data.Read("surface",            &mSurfaceType);
    ok &= data.Read("particle_action",    &mParticleAction);
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
    ok &= ReadLegacyValue<glm::vec2>("color_weight", "kWeight", data);
    ok &= ReadLegacyValue<glm::vec2>("texture_scale", "kTextureScale", data);
    ok &= ReadLegacyValue<glm::vec3>("texture_velocity", "kTextureVelocity", data);
    ok &= ReadLegacyValue<float>("texture_rotation", "kTextureRotation", data);

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
ShaderSource MaterialClass::CreateShaderStub(Type type)
{
    ShaderSource source;
    if (type == Type::Color)
        source = GetColorShaderSource();
    else if (type == Type::Gradient)
        source = GetGradientShaderSource();
    else if (type == Type::Texture)
        source = GetTextureShaderSource();
    else if (type == Type::Sprite)
        source = GetSpriteShaderSource();
    else if (type == Type::Tilemap)
        source = GetTilemapShaderSource();
    else BUG("Bug on shader stub based on shader type.");

    // if the shader doesn't already use these uniforms and
    // varyings then add them now so that they will be made
    // known to the user in the shader stub source.
    // If they are not needed in the shader they can then be
    // simply deleted.

    if (!source.HasUniform("kTime"))
        source.AddUniform("kTime", ShaderSource::UniformType::Float);
    if (!source.HasUniform("kEditingMode"))
        source.AddUniform("kEditingMode", ShaderSource::UniformType::Int);
    if (!source.HasUniform("kRenderPoints"))
        source.AddUniform("kRenderPoints", ShaderSource::UniformType::Float);
    if (!source.HasUniform("kParticleEffect"))
        source.AddUniform("kParticleEffect", ShaderSource::UniformType::Int);
    if (!source.HasUniform("kSurfaceType"))
        source.AddUniform("kSurfaceType", ShaderSource::UniformType::Int);
    if (!source.HasUniform("kDrawPrimitive"))
        source.AddUniform("kDrawPrimitive", ShaderSource::UniformType::Int);
    //if (!source.HasUniform("kRenderPass"))
    //    source.AddUniform("kRenderPass", ShaderSource::UniformType::Int);

    if (!source.HasVarying("vTexCoord"))
        source.AddVarying("vTexCoord",  ShaderSource::VaryingType::Vec2f);
    if (!source.HasVarying("vParticleAlpha"))
        source.AddVarying("vParticleAlpha", ShaderSource::VaryingType::Float);
    if (!source.HasVarying("vParticleRandomValue"))
        source.AddVarying("vParticleRandomValue", ShaderSource::VaryingType::Float);
    if (!source.HasVarying("vParticleTime"))
        source.AddVarying("vParticleTime", ShaderSource::VaryingType::Float);

    //source.SetComment("kRenderPass", "Render pass identifier.");
    source.SetComment("kTime", "Material instance time in seconds.");
    source.SetComment("kEditingMode", "Flag to indicate whether we're in editing mode, i.e. running editor or not.\n"
                      "0 = editing mode is off, i.e. we're running in production/deployment\n"
                      "1 = editing mode is on, i.e. we're running in the editor.");
    source.SetComment("kRenderPoints", "Flag to indicate when the geometry is GL_POINTS.\n"
                      "When rendering GL_POINTS the normal texture coordinates don't apply\n"
                      "but you must use gl_PointCoord GLSL variable.\n"
                      "For convenience this flag is a float with either 0.0 or 1.0 value\n"
                      "so to cover both cases you can do:\n"
                      "   vec2 coordinates = mix(vTexCoords, gl_PointCoord, kRenderPoints);\n"
                      "to be used to get the coordinates always correctly.");
    source.SetComment("kBaseColor", "The base color set for the material.\n"
                                    "Used to modulate the output color (for example the texture sample).\n");
    source.SetComment("kTextureVelocity", "Velocity of texture coordinates. \n"
                                          ".xy = x and y axis velocity in texture units per second.\n"
                                          " .z = rotation in radians per second.");
    source.SetComment("kTextureWrap", "Software texture wrapping mode. Needed when texture sub-rects are used\n"
                                      "and the texture sampling can exceed the sub-rect borders.\n"
                                      "0 = disabled (nothing is done)\n"
                                      "1 = clamp to sub-rect borders\n"
                                      "2 = wrap around the sub-rect borders\n"
                                      "3 = wrap + mirror around the sub-rect borders.\n");
    source.SetComment("kTextureRotation", "Texture coordinate rotation in radians.");
    source.SetComment("kTextureScale", "Texture coordinate scaling factor(s) for both x,y axis.");
    source.SetComment("kTexture", "The current texture to sample from.");
    source.SetComment("kTextureBox", "Texture box (texture sub-rect) that defines a smaller area inside the\n"
                                     "texture for sampling the texture. Needed to support arbitrary texture rects\n"
                                     "and to support texture packing.\n"
                                     ".xy = the translation of the box in normalized texture coordinates\n"
                                     ".zw = the size of the box in normalized texture coordinates.");
    source.SetComment("kAlphaMask", "Flag to indicate whether the texture is an alpha mask or not."
                                    "0.0 = texture is normal color data and has all RGBA channels\n"
                                    "1.0 = texture is an alpha mask and only has A channel");
    source.SetComment("kAlphaCutoff", "Alpha cutoff value to support alpha based cutouts such as sprites\n"
                                      "that aren't really transparent but just cutouts. Cutoff value is the value\n"
                                      "to test against in order to discard the fragments.");
    source.SetComment("kParticleEffect", "Particle effect enumerator value.");
    source.SetComment("kSurfaceType", "Material surface type enumerator value.");

    source.SetComment("vTileData", "Fragment's tile data coming from the vertex shader.\n"
                                   ".x = the material tile index. the product of 'row * cols_per_row + col'\n"
                                   ".y = unused data.");

    source.SetComment("vTexCoord", "Fragment's texture coordinates coming from the vertex shader.");

    source.SetComment("vParticleAlpha", "The per particle alpha value. Only available when rendering particles.");
    source.SetComment("vParticleRandomValue", "The per particle random value. Only available when rendering particles.");
    source.SetComment("vParticleTime", "Normalized per particle lifetime. Only available when rendering particles.\n"
                                       "0.0 = beginning of life\n"
                                       "1.0 = end of life");

    return source;
}

// static
std::string MaterialClass::GetColorUniformName(ColorIndex index)
{
    if (index == ColorIndex::BaseColor)
        return "kBaseColor";
    else if (index == ColorIndex::TopLeft)
        return "kColor0";
    else if (index == ColorIndex::TopRight)
        return "kColor1";
    else if (index == ColorIndex::BottomLeft)
        return "kColor2";
    else if (index == ColorIndex::BottomRight)
        return "kColor3";
    else BUG("Unknown color index.");
    return "";
}

// static
std::unique_ptr<MaterialClass> MaterialClass::ClassFromJson(const data::Reader& data)
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
    std::swap(mParticleAction  , tmp.mParticleAction);
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

    if (state.uniforms)
    {
        if (const auto* active_texture = base::SafeFind(*state.uniforms, std::string("active_texture_map")))
        {
            const auto* map_id = std::get_if<std::string>(active_texture);
            ASSERT(map_id  && "Active texture map selection has wrong (non-string) uniform type.");
            for (auto& map : mTextureMaps)
            {
                if (map->GetId() == *map_id)
                    return map.get();
            }
            if (state.first_render)
                WARN("No such texture map found in material. Falling back on default. [name='%1', map=%2]", mName, *map_id);
        }
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
    // First check the inline source within the material. If that doesn't
    // exist then check the shader URI for loading the shader from the URI.
    // Finally if that doesn't exist and the material has a known type then
    // use one of the built-in shaders!

    if (!mShaderSrc.empty())
        return ShaderSource::FromRawSource(mShaderSrc, ShaderSource::Type::Fragment);

    if (!mShaderUri.empty())
    {
        Loader::ResourceDesc desc;
        desc.uri  = mShaderUri;
        desc.id   = mClassId;
        desc.type = Loader::Type::Shader;
        const auto& buffer = gfx::LoadResource(desc);
        if (!buffer)
        {
            ERROR("Failed to load custom material shader source file. [name='%1', uri='%2']", mName, mShaderUri);
            return ShaderSource();
        }
        const char* beg = (const char*)buffer->GetData();
        const char* end = beg + buffer->GetByteSize();

        DEBUG("Load shader source. [uri=%1', material='%2']", mShaderUri, mName);

        auto source = ShaderSource::FromRawSource(std::string(beg, end), ShaderSource::Type::Fragment);
        source.SetShaderSourceUri(mShaderUri);
        return source;
    }

    if (mType == Type::Color)
        return GetColorShaderSource(state, device);
    else if (mType == Type::Gradient)
        return GetGradientShaderSource(state, device);
    else if (mType == Type::Sprite)
        return GetSpriteShaderSource(state, device);
    else if (mType == Type::Texture)
        return GetTextureShaderSource(state, device);
    else if (mType == Type::Tilemap)
        return GetTilemapShaderSource(state, device);
    else  if (mType == Type::Custom)
    {
        ERROR("Material has no shader source specified. [name='%1']", mName);
        return ShaderSource();
    } else BUG("Unknown material type.");
    return ShaderSource();
}

ShaderSource MaterialClass::GetColorShaderSource(const State& state, const Device& device) const
{
    return GetColorShaderSource();
}

// static
ShaderSource MaterialClass::GetColorShaderSource()
{
    ShaderSource source;
    source.SetShaderUniformAPIVersion(1);
    source.SetType(ShaderSource::Type::Fragment);
    source.SetPrecision(ShaderSource::Precision::High);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.AddUniform("kBaseColor",     ShaderSource::UniformType::Color4f);
    source.AddVarying("vParticleAlpha", ShaderSource::VaryingType::Float);
    source.AddSource(R"(
void FragmentShaderMain()
{
  vec4 color = kBaseColor;
  color.a *= vParticleAlpha;
  fs_out.color = color;
}
)");

    source.SetStub(R"(
void FragmentShaderMain() {

    // your code here

    fs_out.color = kBaseColor * vParticleAlpha;
}
)");

    return source;
}


ShaderSource MaterialClass::GetGradientShaderSource(const State& state, const Device& device) const
{
    return GetGradientShaderSource();
}

// static
ShaderSource MaterialClass::GetGradientShaderSource()
{
    ShaderSource source;
    source.SetShaderUniformAPIVersion(1);
    source.SetType(ShaderSource::Type::Fragment);
    source.SetPrecision(ShaderSource::Precision::High);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.AddUniform("kColor0", ShaderSource::UniformType::Color4f,
                      "The gradient top left color value.");
    source.AddUniform("kColor1", ShaderSource::UniformType::Color4f,
                      "The gradient top right color value.");
    source.AddUniform("kColor2", ShaderSource::UniformType::Color4f,
                      "The gradient bottom left color value.");
    source.AddUniform("kColor3", ShaderSource::UniformType::Color4f,
                      "The gradient bottom right color value.");
    source.AddUniform("kOffset", ShaderSource::UniformType::Vec2f,
                      "The gradient x and y axis mixing/blending coefficient offset.");
    source.AddUniform("kRenderPoints", ShaderSource::UniformType::Float);

    source.AddVarying("vTexCoord", ShaderSource::VaryingType::Vec2f);
    source.AddVarying("vParticleAlpha", ShaderSource::VaryingType::Float);

    source.AddSource(R"(
vec4 MixGradient(vec2 coords)
{
  vec4 top = mix(kColor0, kColor1, coords.x);
  vec4 bot = mix(kColor2, kColor3, coords.x);
  vec4 color = mix(top, bot, coords.y);
  return color;
}

void FragmentShaderMain()
{
  vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
  coords = (coords - kOffset) + vec2(0.5, 0.5);
  coords = clamp(coords, vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec4 color  = MixGradient(coords);

  fs_out.color.rgb = vec3(pow(color.rgb, vec3(2.2)));
  fs_out.color.a   = color.a * vParticleAlpha;
}
)");

    source.SetStub(R"(
void FragmentShaderMain() {

    // your code here

    vec4 color_mix_0 = mix(kColor0, kColor1, vTexCoord.x);
    vec4 color_mix_1 = mix(kColor2, kColor3, vTexCoord.x);
    fs_out.color = mix(color_mix_0, color_mix_1, vTexCoord.y);
}
)");


    return source;
}

ShaderSource MaterialClass::GetSpriteShaderSource(const State& state, const Device& device) const
{
    return GetSpriteShaderSource();
}

ShaderSource MaterialClass::GetSpriteShaderSource()
{
    // todo: maybe pack some of shader uniforms

    ShaderSource source;
    source.SetShaderUniformAPIVersion(1);
    source.SetType(ShaderSource::Type::Fragment);
    source.SetPrecision(ShaderSource::Precision::High);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.AddUniform("kTexture0", ShaderSource::UniformType::Sampler2D);
    source.AddUniform("kTexture1", ShaderSource::UniformType::Sampler2D);
    source.AddUniform("kTextureBox0", ShaderSource::UniformType::Vec4f);
    source.AddUniform("kTextureBox1", ShaderSource::UniformType::Vec4f);
    source.AddUniform("kBaseColor", ShaderSource::UniformType::Color4f);
    source.AddUniform("kRenderPoints", ShaderSource::UniformType::Float);
    source.AddUniform("kAlphaCutoff", ShaderSource::UniformType::Float);
    source.AddUniform("kTime", ShaderSource::UniformType::Float);
    source.AddUniform("kBlendCoeff", ShaderSource::UniformType::Float);
    source.AddUniform("kParticleEffect", ShaderSource::UniformType::Int);
    source.AddUniform("kTextureScale", ShaderSource::UniformType::Vec2f);
    source.AddUniform("kTextureVelocity", ShaderSource::UniformType::Vec3f);
    source.AddUniform("kTextureRotation", ShaderSource::UniformType::Float);
    source.AddUniform("kTextureWrap", ShaderSource::UniformType::Vec2i); // 0 disabled, 1 clamp, 2 wrap, 3 repeat
    source.AddUniform("kAlphaMask", ShaderSource::UniformType::Vec2f);

    source.AddVarying("vTexCoord", ShaderSource::VaryingType::Vec2f);
    source.AddVarying("vParticleRandomValue", ShaderSource::VaryingType::Float);
    source.AddVarying("vParticleAlpha", ShaderSource::VaryingType::Float);

    source.AddSource(R"(

// do software wrapping for texture coordinates.
// used for cases when texture sub-rects are used.
float Wrap(float x, float w, int mode) {
    if (mode == 1) {
        return clamp(x, 0.0, w);
    } else if (mode == 2) {
        return fract(x / w) * w;
    } else if (mode == 3) {
        float p = floor(x / w);
        float f = fract(x / w);
        int m = int(floor(mod(p, 2.0)));
        if (m == 0)
           return f * w;

        return w - f * w;
    }
    return x;
}

vec2 WrapTextureCoords(vec2 coords, vec2 box)
{
  float x = Wrap(coords.x, box.x, kTextureWrap.x);
  float y = Wrap(coords.y, box.y, kTextureWrap.y);
  return vec2(x, y);
}

vec2 RotateCoords(vec2 coords)
{
    float random_angle = 0.0;
    if (kParticleEffect == 2)
        random_angle = mix(0.0, 3.1415926, vParticleRandomValue);

    float angle = kTextureRotation + kTextureVelocity.z * kTime + random_angle;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

void FragmentShaderMain()
{
    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    // note about gl_PointCoord:
    // "However, the gl_PointCoord fragment shader input defines
    // a per-fragment coordinate space (s, t) where s varies from
    // 0 to 1 across the point horizontally left-to-right, and t
    // ranges from 0 to 1 across the point vertically top-to-bottom."
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = RotateCoords(coords);

    coords += kTextureVelocity.xy * kTime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex0 = kTextureBox0.zw;
    vec2 scale_tex1 = kTextureBox1.zw;
    vec2 trans_tex0 = kTextureBox0.xy;
    vec2 trans_tex1 = kTextureBox1.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    vec2 c1 = WrapTextureCoords(coords * scale_tex0, scale_tex0) + trans_tex0;
    vec2 c2 = WrapTextureCoords(coords * scale_tex1, scale_tex1) + trans_tex1;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 tex0 = texture(kTexture0, c1);
    vec4 tex1 = texture(kTexture1, c2);

    vec4 col0 = mix(kBaseColor * tex0, vec4(kBaseColor.rgb, kBaseColor.a * tex0.a), kAlphaMask[0]);
    vec4 col1 = mix(kBaseColor * tex1, vec4(kBaseColor.rgb, kBaseColor.a * tex1.a), kAlphaMask[1]);

    vec4 color = mix(col0, col1, kBlendCoeff);
    color.a *= vParticleAlpha;

    if (color.a <= kAlphaCutoff)
        discard;

    fs_out.color = color;
}
)");

    source.SetStub(R"(
void FragmentShaderMain() {

    // your code here

    fs_out.color = mix(texture(kTexture0, vTexCoord),
                       texture(kTexture1, vTexCoord),
                       kBlendCoeff) * kBaseColor;
}
)");

    return source;
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
    program.SetUniform("kBlendCoeff",                  kBlendCoeff);
    program.SetUniform("kAlphaMask",                   alpha_mask);

    // set software wrap/clamp. 0 = disabled.
    if (need_software_wrap)
    {
        auto ToGLSL = [](TextureWrapping wrapping) {
            if (wrapping == TextureWrapping::Clamp)
                return 1;
            else if (wrapping == TextureWrapping::Repeat)
                return 2;
            else if (wrapping == TextureWrapping::Mirror)
                return 3;
            else BUG("Bug on software texture wrap.");
        };
        program.SetUniform("kTextureWrap", ToGLSL(mTextureWrapY), ToGLSL(mTextureWrapY));
    }
    else
    {
        program.SetUniform("kTextureWrap", 0, 0);
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

ShaderSource MaterialClass::GetTextureShaderSource(const State& state, const Device& device) const
{
    return GetTextureShaderSource();
}

// static
ShaderSource MaterialClass::GetTextureShaderSource()
{
    // todo: pack some of the uniforms ?

    ShaderSource source;
    source.SetShaderUniformAPIVersion(1);
    source.SetType(ShaderSource::Type::Fragment);
    source.SetPrecision(ShaderSource::Precision::High);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.AddUniform("kTexture", ShaderSource::UniformType::Sampler2D);
    source.AddUniform("kTextureBox", ShaderSource::UniformType::Vec4f);
    source.AddUniform("kAlphaMask", ShaderSource::UniformType::Float);
    source.AddUniform("kRenderPoints", ShaderSource::UniformType::Float);
    source.AddUniform("kAlphaCutoff", ShaderSource::UniformType::Float);
    source.AddUniform("kParticleEffect", ShaderSource::UniformType::Int);
    source.AddUniform("kTime", ShaderSource::UniformType::Float);
    source.AddUniform("kTextureScale", ShaderSource::UniformType::Vec2f);
    source.AddUniform("kTextureVelocity", ShaderSource::UniformType::Vec3f);
    source.AddUniform("kTextureRotation", ShaderSource::UniformType::Float);
    source.AddUniform("kBaseColor", ShaderSource::UniformType::Color4f);
    source.AddUniform("kTextureWrap", ShaderSource::UniformType::Vec2i); // 0 disabled, 1 clamp, 2 wrap, 3 repeat

    source.AddVarying("vTexCoord", ShaderSource::VaryingType::Vec2f);
    source.AddVarying("vParticleRandomValue", ShaderSource::VaryingType::Float);
    source.AddVarying("vParticleAlpha", ShaderSource::VaryingType::Float);

    source.AddSource(R"(
// do software wrapping for texture coordinates.
// used for cases when texture sub-rects are used.
float Wrap(float x, float w, int mode) {
    if (mode == 1) {
        return clamp(x, 0.0, w);
    } else if (mode == 2) {
        return fract(x / w) * w;
    } else if (mode == 3) {
        float p = floor(x / w);
        float f = fract(x / w);
        int m = int(floor(mod(p, 2.0)));
        if (m == 0)
           return f * w;

        return w - f * w;
    }
    return x;
}

vec2 WrapTextureCoords(vec2 coords, vec2 box)
{
  float x = Wrap(coords.x, box.x, kTextureWrap.x);
  float y = Wrap(coords.y, box.y, kTextureWrap.y);
  return vec2(x, y);
}

vec2 RotateCoords(vec2 coords)
{
    float random_angle = 0.0;
    if (kParticleEffect == 2)
        random_angle = mix(0.0, 3.1415926, vParticleRandomValue);

    float angle = kTextureRotation + kTextureVelocity.z * kTime + random_angle;
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}

void FragmentShaderMain()
{
    // for texture coords we need either the coords from the
    // vertex data or gl_PointCoord if the geometry is being
    // rasterized as points.
    // we set kRenderPoints to 1.0f when rendering points.
    // note about gl_PointCoord:
    // "However, the gl_PointCoord fragment shader input defines
    // a per-fragment coordinate space (s, t) where s varies from
    // 0 to 1 across the point horizontally left-to-right, and t
    // ranges from 0 to 1 across the point vertically top-to-bottom."
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = RotateCoords(coords);
    coords += kTextureVelocity.xy * kTime;
    coords = coords * kTextureScale;

    // apply texture box transformation.
    vec2 scale_tex = kTextureBox.zw;
    vec2 trans_tex = kTextureBox.xy;

    // scale and transform based on texture box. (todo: maybe use texture matrix?)
    coords = WrapTextureCoords(coords * scale_tex, scale_tex) + trans_tex;

    // sample textures, if texture is a just an alpha mask we use
    // only the alpha channel later.
    vec4 texel = texture(kTexture, coords);

    // either modulate/mask texture color with base color
    // or modulate base color with texture's alpha value if
    // texture is an alpha mask
    vec4 color = mix(kBaseColor * texel, vec4(kBaseColor.rgb, kBaseColor.a * texel.a), kAlphaMask);
    color.a *= vParticleAlpha;

    if (color.a <= kAlphaCutoff)
        discard;

    fs_out.color = color;
}
)");

    source.SetStub(R"(
void FragmentShaderMain() {

    // your code here

    fs_out.color = texture(kTexture, vTexCoord) * kBaseColor;

}
)");

    return source;
}

ShaderSource MaterialClass::GetTilemapShaderSource(const State& state, const Device& device) const
{
    return GetTilemapShaderSource();
}

// static
ShaderSource MaterialClass::GetTilemapShaderSource()
{
    ShaderSource source;
    source.SetShaderUniformAPIVersion(1);
    source.SetType(ShaderSource::Type::Fragment);
    source.SetPrecision(ShaderSource::Precision::High);
    source.SetVersion(ShaderSource::Version::GLSL_300);
    source.AddUniform("kTexture", ShaderSource::UniformType::Sampler2D);
    source.AddUniform("kTextureBox", ShaderSource::UniformType::Vec4f);
    source.AddUniform("kAlphaCutoff", ShaderSource::UniformType::Float);
    source.AddUniform("kTime", ShaderSource::UniformType::Float);
    source.AddUniform("kBaseColor", ShaderSource::UniformType::Color4f);
    source.AddUniform("kTileIndex", ShaderSource::UniformType::Float);
    source.AddUniform("kTileSize", ShaderSource::UniformType::Vec2f);
    source.AddUniform("kTileOffset", ShaderSource::UniformType::Vec2f);
    source.AddUniform("kTilePadding", ShaderSource::UniformType::Vec2f);
    source.AddUniform("kRenderPoints", ShaderSource::UniformType::Float);

    source.AddVarying("vTexCoord", ShaderSource::VaryingType::Vec2f);
    source.AddVarying("vTileData", ShaderSource::VaryingType::Vec2f);

    source.AddSource(R"(
float GetTileIndex() {
    // to help debugging the material in the editor we're checking the flag
    // here whether we're editing or not and then either return a tile index
    // based on a uniform or a "real" tile index based on tile data.
    if (kTileIndex > 0.0)
        return kTileIndex - 1.0;
    return vTileData.x;
}

void FragmentShaderMain()
{
    // the tile rendering can provide geometry also through GL_POINTS.
    // that means we must then use gl_PointCoord as the texture coordinates
    // for the fragment.
    vec2 frag_texture_coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    // apply texture box transformation
    vec2 texture_box_size      = kTextureBox.zw;
    vec2 texture_box_translate = kTextureBox.xy;

    vec2 texture_size = vec2(textureSize(kTexture, 0));

    vec2 tile_texture_offset   = kTileOffset / texture_size;
    vec2 tile_texture_size     = kTileSize / texture_size;
    vec2 tile_texture_padding  = kTilePadding / texture_size;
    vec2 tile_texture_box_size = tile_texture_size + (tile_texture_padding * 2.0);

    vec2 tile_map_size = texture_box_size - tile_texture_offset;
    vec2 tile_map_dims = tile_map_size / tile_texture_box_size; // rows and cols

    int tile_index = int(GetTileIndex());
    int tile_cols = int(tile_map_dims.x);
    int tile_rows = int(tile_map_dims.y);
    int tile_row = tile_index / tile_cols;
    int tile_col = tile_index - (tile_row * tile_cols);

    // build the final texture coordinate by successively adding offsets
    // in order to map the frag coord to the right texture position inside
    // the tile, inside the tile region of the tile-texture inside the
    // texture box inside the texture atlas.
    vec2 texture_coords = vec2(0.0, 0.0);

    // add texture box offset inside the texture.
    texture_coords += texture_box_translate;

    // add the tile offset inside the texture box to get to the tiles
    texture_coords += tile_texture_offset;

    // add the tile base coord to get to the top-left of
    // the tile coordinate.
    texture_coords += vec2(float(tile_col), float(tile_row)) * tile_texture_box_size;

    texture_coords += tile_texture_padding;

    // add the frag coordinate relative to the tile's top left
    texture_coords += tile_texture_size * frag_texture_coords;

    vec4 texture_sample = texture(kTexture, texture_coords);

    // produce color value.
    vec4 color = texture_sample * kBaseColor;

    if (color.a <= kAlphaCutoff)
        discard;

    fs_out.color = color;
}
)");
    return source;

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

    program.SetTexture("kTexture", 0, *texture);
    program.SetUniform("kTextureBox", x, y, sx, sy);
    program.SetTextureCount(1);
    program.SetUniform("kAlphaMask", binds.textures[0]->IsAlphaMask() ? 1.0f : 0.0f);

    // set software wrap/clamp. 0 = disabled.
    if (need_software_wrap)
    {
        auto ToGLSL = [](TextureWrapping wrapping) {
            if (wrapping == TextureWrapping::Clamp)
                return 1;
            else if (wrapping == TextureWrapping::Repeat)
                return 2;
            else if (wrapping == TextureWrapping::Mirror)
                return 3;
            else BUG("Bug on software texture wrap.");
        };
        program.SetUniform("kTextureWrap", ToGLSL(mTextureWrapX), ToGLSL(mTextureWrapY));
    }
    else
    {
        program.SetUniform("kTextureWrap", 0, 0);
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

    if (state.editing_mode)
    {
        SetUniform("kTileIndex", state.uniforms, GetUniformValue("kTileIndex", 0.0f), program);
    }
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
    material.SetColor(top_left, GradientClass::ColorIndex::TopLeft);
    material.SetColor(top_right, GradientClass::ColorIndex::TopRight);
    material.SetColor(bottom_left, GradientClass::ColorIndex::BottomLeft);
    material.SetColor(bottom_right, GradientClass::ColorIndex::BottomRight);
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

