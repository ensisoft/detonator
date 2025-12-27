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

#include <algorithm>  // for swap

#include "base/assert.h"
#include "base/utility.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "graphics/texture_source.h"
#include "graphics/texture_map.h"
#include "graphics/texture_texture_source.h"
#include "graphics/texture_file_source.h"
#include "graphics/texture_bitmap_buffer_source.h"
#include "graphics/texture_bitmap_generator_source.h"
#include "graphics/texture_text_buffer_source.h"

namespace gfx
{

TextureMap::TextureMap(const TextureMap& other, bool copy)
{
    mId = copy ? other.mId : base::RandomString(10);
    mName               = other.mName;
    mType               = other.mType;
    mFps                = other.mFps;
    mLooping            = other.mLooping;
    mSamplerName[0]     = other.mSamplerName[0];
    mSamplerName[1]     = other.mSamplerName[1];
    mRectUniformName[0] = other.mRectUniformName[0];
    mRectUniformName[1] = other.mRectUniformName[1];
    mSpriteSheet        = other.mSpriteSheet;
    for (const auto& texture : other.mTextures)
    {
        TextureMapping dupe;
        dupe.rect = texture.rect;
        if (texture.source)
            dupe.source = copy ? texture.source->Copy() : texture.source->Clone();
        mTextures.push_back(std::move(dupe));
    }
}

TextureMap::TextureMap(TextureMap&& other) noexcept
{
    mId = std::move(other.mId);
    mName = std::move(other.mName);
    mType = other.mType;
    mFps = other.mFps;
    mLooping = other.mLooping;
    mSamplerName[0] = std::move(other.mSamplerName[0]);
    mSamplerName[1] = std::move(other.mSamplerName[1]);
    mRectUniformName[0] = std::move(other.mRectUniformName[0]);
    mRectUniformName[1] = std::move(other.mRectUniformName[1]);
    mSpriteSheet = std::move(other.mSpriteSheet);
    mTextures = std::move(other.mTextures);
}

size_t TextureMap::GetHash() const noexcept
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mType);
    hash = base::hash_combine(hash, mFps);
    hash = base::hash_combine(hash, mSamplerName[0]);
    hash = base::hash_combine(hash, mSamplerName[1]);
    hash = base::hash_combine(hash, mRectUniformName[0]);
    hash = base::hash_combine(hash, mRectUniformName[1]);
    hash = base::hash_combine(hash, mLooping);
    hash = base::hash_combine(hash, mSpriteSheet);
    for (const auto& texture : mTextures)
    {
        const auto* source = texture.source.get();
        hash = base::hash_combine(hash, source ? source->GetHash() : 0);
        hash = base::hash_combine(hash, texture.rect);
    }
    return hash;
}

bool TextureMap::BindTextures(const BindingState& state, Device& device, BoundState& result) const
{
    if (mTextures.empty())
        return false;

    if (mType == Type::Sprite)
    {
        const auto frame_interval = 1.0f / std::max(mFps, 0.001f);
        const auto frame_fraction = std::fmod(state.current_time, frame_interval);
        const auto blend_coeff = frame_fraction/frame_interval;
        const auto first_index = (unsigned)(state.current_time/frame_interval);
        const auto frame_count =  GetSpriteFrameCount();
        const auto max_index = frame_count - 1;
        const auto texture_count = std::min(frame_count, 2u);
        const auto first_frame  = mLooping
                                  ? ((first_index + 0) % frame_count)
                                  : math::clamp(0u, max_index, first_index+0);
        const auto second_frame = mLooping
                                  ? ((first_index + 1) % frame_count)
                                  : math::clamp(0u, max_index, first_index+1);
        const unsigned frame_index[2] = {
                first_frame, second_frame
        };

        TextureSource::Environment texture_source_env;
        texture_source_env.dynamic_content = state.dynamic_content;

        // if we're using a single sprite sheet then the sprite frames
        // are sub-rects inside the first texture rect.
        if (const auto* spritesheet = GetSpriteSheet())
        {
            const auto& sprite = mTextures[0];
            const auto& source = sprite.source;
            const auto& rect   = sprite.rect;
            const auto tile_width = rect.GetWidth() / spritesheet->cols;
            const auto tile_height = rect.GetHeight() / spritesheet->rows;
            const auto tile_xpos = rect.GetX();
            const auto tile_ypos = rect.GetY();

            if (auto* texture = source->Upload(texture_source_env, device))
            {
                for (unsigned i = 0; i < 2; ++i)
                {
                    const auto tile_index = frame_index[i];
                    const auto tile_row = tile_index / spritesheet->cols;
                    const auto tile_col = tile_index % spritesheet->cols;
                    FRect tile_rect;
                    tile_rect.Resize(tile_width, tile_height);
                    tile_rect.Move(tile_xpos, tile_ypos);
                    tile_rect.Translate(tile_col * tile_width, tile_row * tile_height);

                    result.textures[i]      = texture;
                    result.rects[i]         = tile_rect;
                    result.sampler_names[i] = mSamplerName[i];
                    result.rect_names[i]    = mRectUniformName[i];
                }
            } else return false;
        }
        else
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                const auto& sprite = mTextures[frame_index[i]];
                const auto& source = sprite.source;
                if (auto* texture = source->Upload(texture_source_env, device))
                {
                    result.textures[i]      = texture;
                    result.rects[i]         = sprite.rect;
                    result.sampler_names[i] = mSamplerName[i];
                    result.rect_names[i]    = mRectUniformName[i];
                } else return false;
            }
        }
        result.blend_coefficient = blend_coeff;
    }
    else if (mType == Type::Texture2D)
    {
        TextureSource::Environment texture_source_env;
        texture_source_env.dynamic_content = state.dynamic_content;

        if (auto* texture = mTextures[0].source->Upload(texture_source_env, device))
        {
            result.textures[0]       = texture;
            result.rects[0]          = mTextures[0].rect;
            result.sampler_names[0]  = mSamplerName[0];
            result.rect_names[0]     = mRectUniformName[0];
            result.blend_coefficient = 0;
        } else  return false;
    }
    return true;
}


void TextureMap::IntoJson(data::Writer& data) const
{
    data.Write("id",            mId);
    data.Write("name",          mName);
    data.Write("type",          mType);
    data.Write("fps",           mFps);
    data.Write("sampler_name0", mSamplerName[0]);
    data.Write("sampler_name1", mSamplerName[1]);
    data.Write("rect_name0",    mRectUniformName[0]);
    data.Write("rect_name1",    mRectUniformName[1]);
    data.Write("looping",       mLooping);
    if (const auto* spritesheet = GetSpriteSheet())
    {
        data.Write("spritesheet_rows", spritesheet->rows);
        data.Write("spritesheet_cols", spritesheet->cols);
    }

    for (const auto& texture : mTextures)
    {
        auto chunk = data.NewWriteChunk();
        if (texture.source)
            texture.source->IntoJson(*chunk);
        ASSERT(!chunk->HasValue("type"));
        ASSERT(!chunk->HasValue("box"));
        chunk->Write("type",  texture.source->GetSourceType());
        chunk->Write("rect",  texture.rect);
        data.AppendChunk("textures", std::move(chunk));
    }
}

bool TextureMap::FromJson(const data::Reader& data)
{
    bool ok = true;
    if (data.HasValue("id"))
        ok &= data.Read("id", &mId);
    if (data.HasValue("name"))
        ok &= data.Read("name", &mName);

    ok &= data.Read("type",          &mType);
    ok &= data.Read("fps",           &mFps);
    ok &= data.Read("sampler_name0", &mSamplerName[0]);
    ok &= data.Read("sampler_name1", &mSamplerName[1]);
    ok &= data.Read("rect_name0",    &mRectUniformName[0]);
    ok &= data.Read("rect_name1",    &mRectUniformName[1]);
    ok &= data.Read("looping",       &mLooping);

    SpriteSheet spritesheet;
    if (data.HasValue("spritesheet_rows") &&
        data.HasValue("spritesheet_cols"))
    {
        ok &= data.Read("spritesheet_rows", &spritesheet.rows);
        ok &= data.Read("spritesheet_cols", &spritesheet.cols);
        mSpriteSheet = spritesheet;
    }

    for (unsigned i=0; i<data.GetNumChunks("textures"); ++i)
    {
        const auto& chunk = data.GetReadChunk("textures", i);
        TextureSource::Source type;
        if (chunk->Read("type", &type))
        {
            std::unique_ptr<TextureSource> source;
            if (type == TextureSource::Source::Filesystem)
                source = std::make_unique<TextureFileSource>();
            else if (type == TextureSource::Source::TextBuffer)
                source = std::make_unique<TextureTextBufferSource>();
            else if (type == TextureSource::Source::BitmapBuffer)
                source = std::make_unique<TextureBitmapBufferSource>();
            else if (type == TextureSource::Source::BitmapGenerator)
                source = std::make_unique<TextureBitmapGeneratorSource>();
            else if (type == TextureSource::Source::Texture)
                source = std::make_unique<TextureTextureSource>();
            else BUG("Unhandled texture source type.");

            TextureMapping texture;
            ok &= source->FromJson(*chunk);
            ok &= chunk->Read("rect", &texture.rect);
            texture.source = std::move(source);
            mTextures.push_back(std::move(texture));
        }
    }
    return ok;
}

bool TextureMap::FromLegacyJsonTexture2D(const data::Reader& data)
{
    bool ok = true;
    FRect rect;
    ok &= data.Read("rect",         &rect);
    ok &= data.Read("sampler_name", &mSamplerName[0]);
    ok &= data.Read("rect_name",    &mRectUniformName[0]);

    const auto& texture = data.GetReadChunk("texture");
    if (!texture)
        return false;

    TextureSource::Source type;
    if (!texture->Read("type", &type))
        return false;

    std::unique_ptr<TextureSource> source;
    if (type == TextureSource::Source::Filesystem)
        source = std::make_unique<TextureFileSource>();
    else if (type == TextureSource::Source::TextBuffer)
        source = std::make_unique<TextureTextBufferSource>();
    else if (type == TextureSource::Source::BitmapBuffer)
        source = std::make_unique<TextureBitmapBufferSource>();
    else if (type == TextureSource::Source::BitmapGenerator)
        source = std::make_unique<TextureBitmapGeneratorSource>();
    else if (type == TextureSource::Source::Texture)
        source = std::make_unique<TextureTextureSource>();
    else BUG("Unhandled texture source type.");

    if (!source->FromJson(*texture))
        return false;

    if (mTextures.empty())
        mTextures.resize(1);
    mTextures[0].source = std::move(source);
    mTextures[0].rect   = rect;
    return ok;
}

size_t TextureMap::FindTextureSourceIndexById(const std::string& id) const
{
    size_t i=0;
    for (i=0; i<GetNumTextures(); ++i)
    {
        auto* source = GetTextureSource(i);
        if (source->GetId() == id)
            break;
    }
    return i;
}

size_t TextureMap::FindTextureSourceIndexByName(const std::string& name) const
{
    unsigned i=0;
    for (i=0; i<GetNumTextures(); ++i)
    {
        auto* source = GetTextureSource(i);
        if (source->GetName() == name)
            break;
    }
    return i;
}

void TextureMap::SwapSources(size_t one, size_t two)
{
    ASSERT(one < mTextures.size());
    ASSERT(two < mTextures.size());
    if (one == two)
        return;

    std::swap(mTextures[one], mTextures[two]);
}

void TextureMap::ShuffleSource(size_t from_index, size_t to_index)
{
    ASSERT(from_index < mTextures.size());
    ASSERT(to_index < mTextures.size());
    if (from_index == to_index)
        return;

    auto temp = std::move(mTextures[from_index]);
    mTextures.erase(mTextures.begin() + from_index);
    mTextures.insert(mTextures.begin() + to_index, std::move(temp));
}

unsigned TextureMap::GetSpriteFrameCount() const
{
    if (!IsSpriteMap())
        return 0;

    if (const auto* ptr = GetSpriteSheet())
        return ptr->cols * ptr->rows;

    return mTextures.size();
}

float TextureMap::GetSpriteCycleDuration() const
{
    if (!IsSpriteMap())
        return 0.0f;

    const auto sprite_frame_count = (float)GetSpriteFrameCount();
    const auto sprite_fps = GetSpriteFrameRate();
    return sprite_frame_count  / sprite_fps;
}

void TextureMap::SetSpriteFrameRateFromDuration(float duration)
{
    if (duration <= 0.0f)
        return;

    if (!IsSpriteMap())
        return;

    const auto sprite_fps = (float)GetSpriteFrameRate();
    const auto texture_count = (float)GetSpriteFrameCount();
    const auto target_fps = texture_count / duration;
    SetSpriteFrameRate(target_fps);
}

TextureMap& TextureMap::operator=(const TextureMap& other)
{
    if (this == &other)
        return *this;
    TextureMap tmp(other, true);
    std::swap(mId,                 tmp.mId);
    std::swap(mName,               tmp.mName);
    std::swap(mType,               tmp.mType);
    std::swap(mFps,                tmp.mFps);
    std::swap(mTextures,           tmp.mTextures);
    std::swap(mSamplerName[0],     tmp.mSamplerName[0]);
    std::swap(mSamplerName[1],     tmp.mSamplerName[1]);
    std::swap(mRectUniformName[0], tmp.mRectUniformName[0]);
    std::swap(mRectUniformName[1], tmp.mRectUniformName[1]);
    std::swap(mLooping,            tmp.mLooping);
    std::swap(mSpriteSheet,        tmp.mSpriteSheet);
    return *this;
}

} // namespace