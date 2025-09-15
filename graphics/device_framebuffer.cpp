// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include "base/assert.h"
#include "base/logging.h"
#include "graphics/device_texture.h"
#include "graphics/device_framebuffer.h"

namespace gfx {

DeviceFramebuffer::~DeviceFramebuffer()
{
    mColorTextures.clear();

    if (mFramebuffer.IsValid())
    {
        mDevice->DeleteFramebuffer(mFramebuffer);
        DEBUG("Deleted frame buffer object. [name='%1']", mName);
    }
}

void DeviceFramebuffer::SetConfig(const Config& conf)
{
    const auto format = conf.format;
    ASSERT(format != Format::Invalid);
    if (format == Format::DepthTexture32f)
    {
        ASSERT(conf.color_target_count == 0);
        ASSERT(conf.msaa == MSAA::Disabled);
        if (mFramebuffer.IsValid())
        {
            ASSERT(mConfig.format == conf.format);
        }
        mConfig = conf;
    }
    else
    {
        ASSERT(conf.color_target_count >= 1);

        // we don't allow the config to be changed after it has been created.
        // todo: delete the SetConfig API and take the FBO config in the
        // device level API to create the FBO.
        if (mFramebuffer.IsValid())
        {
            ASSERT(mConfig.format == conf.format);
            ASSERT(mConfig.msaa == conf.msaa);
            ASSERT(mConfig.color_target_count == conf.color_target_count);

            // the size can change after the FBO has been created
            // but only when the format is ColorRGBA8. In other wors
            // there are no render buffer attachments.
            ASSERT(mConfig.format == Format::ColorRGBA8);
        }

        mConfig = conf;
        mClientColorTextures.resize(mConfig.color_target_count);
        mColorTextures.resize(mConfig.color_target_count);
    }
}

void DeviceFramebuffer::SetColorTarget(gfx::Texture* texture, ColorAttachment attachment)
{
    const auto index = static_cast<uint8_t>(attachment);

    ASSERT(mConfig.format != Format::Invalid);
    ASSERT(mConfig.format != Format::DepthTexture32f);
    ASSERT(index < mConfig.color_target_count);

    if (texture == mClientColorTextures[index])
        return;

    mClientColorTextures[index] = static_cast<gfx::DeviceTexture*>(texture);

    // if we have a client texture the client texture drives the FBO size.
    // Otherwise the FBO size is based on the size set in the FBO config.
    //
    // The render target (and the resolve target) textures are allowed
    // to change during the lifetime of the FBO but when the texture is
    // changed after the FBO has been created the texture size must match
    // the size used to create the other attachments (if any)

    if (mClientColorTextures[index])
    {
        const auto width  = mClientColorTextures[index]->GetWidth();
        const auto height = mClientColorTextures[index]->GetHeight();

        // don't allow zero size texture.
        ASSERT(width && height);

        // if the FBO has been created and the format is such that there
        // are other attachments then the client texture size must
        // match the size of the other attachments. otherwise the FBO
        // is in invalid state.
        if (mFramebuffer.IsValid() && (mConfig.format != Format::ColorRGBA8))
        {
            ASSERT(width == mConfig.width);
            ASSERT(height == mConfig.height);
        }
    }

    // check that every client provided texture has the same size.
    unsigned width  = 0;
    unsigned height = 0;
    for (size_t i=0; i<mClientColorTextures.size(); ++i)
    {
        if (!mClientColorTextures[i])
            continue;

        if (width == 0 && height == 0)
        {
            width = mClientColorTextures[i]->GetWidth();
            height = mClientColorTextures[i]->GetHeight();
        }
        else
        {
            ASSERT(mClientColorTextures[i]->GetWidth() == width);
            ASSERT(mClientColorTextures[i]->GetHeight() == height);
        }
    }
}

void DeviceFramebuffer::SetDepthTarget(gfx::Texture* texture)
{
    ASSERT(mConfig.format == Format::DepthTexture32f);
    if (mClientDepthTexture == texture)
        return;

    mClientDepthTexture = static_cast<DeviceTexture*>(texture);
}

void DeviceFramebuffer::Resolve(gfx::Texture** color, ColorAttachment attachment) const
{
    const auto index = static_cast<uint8_t>(attachment);
    ASSERT(mConfig.format != Format::Invalid);
    ASSERT(mConfig.format != Format::DepthTexture32f);

    // resolve the MSAA render buffer into a texture target with glBlitFramebuffer
    // the insane part here is that we need a *another* frame buffer for resolving
    // the multisampled color attachment into a texture.
    if (const auto samples = mFramebuffer.samples)
    {
        auto* resolve_target = GetColorBufferTexture(index);
        mDevice->ResolveFramebuffer(mFramebuffer, resolve_target->GetTexture(), index);
        if (color)
            *color = resolve_target;
    }
    else
    {
        auto* texture = GetColorBufferTexture(index);
        if (color)
            *color = texture;
    }
}

unsigned DeviceFramebuffer::GetWidth() const
{
    if (mClientColorTextures.empty())
        return mConfig.width;

    return mClientColorTextures[0]->GetWidth();
}

unsigned DeviceFramebuffer::GetHeight() const
{
    if (mClientColorTextures.empty())
        return mConfig.height;

    return mClientColorTextures[0]->GetHeight();
}

gfx::DeviceTexture* DeviceFramebuffer::GetColorBufferTexture(unsigned index) const noexcept
{
    if (mClientColorTextures[index])
        return mClientColorTextures[index];

    return mColorTextures[index].get();
}

DeviceTexture* DeviceFramebuffer::GetDepthBufferTexture() const noexcept
{
    if (mClientDepthTexture)
        return mClientDepthTexture;

    return mDepthTexture.get();
}

bool DeviceFramebuffer::Complete()
{
    std::vector<unsigned> color_attachments;

    if (mConfig.format == Format::DepthTexture32f)
    {
        CreateDepthBufferTexture();

        auto* depth_target = GetDepthBufferTexture();

        mDevice->BindDepthRenderTargetTexture2D(mFramebuffer, depth_target->GetTexture());
    }
    else
    {
        // in case of a multisampled FBO the color attachment is a multisampled render buffer
        // and the resolve client texture will be the *resolve* target in the blit framebuffer operation.
        if (const auto samples = mFramebuffer.samples)
        {
            CreateColorBufferTextures();
            const auto* resolve_target = GetColorBufferTexture(0);
            const unsigned width  = resolve_target->GetWidth();
            const unsigned height = resolve_target->GetHeight();

            for (unsigned i=0; i<mConfig.color_target_count; ++i)
            {
                mDevice->AllocateMSAAColorRenderTarget(mFramebuffer, i, width, height);
            }
        }
        else
        {
            CreateColorBufferTextures();
            for (unsigned i=0; i<mConfig.color_target_count; ++i)
            {
                auto* color_target = GetColorBufferTexture(i);
                // in case of a single sampled FBO the resolve target can be used directly
                // as the color attachment in the FBO.
                mDevice->BindColorRenderTargetTexture2D(mFramebuffer, color_target->GetTexture(), i);
            }
        }

        for (unsigned i=0; i<mConfig.color_target_count; ++i)
            color_attachments.push_back(i);
    }

    if (!mDevice->CompleteFramebuffer(mFramebuffer, color_attachments))
    {
        ERROR("Unsupported framebuffer configuration. [name='%1']", mName);
        return false;
    }

    return true;
}

bool DeviceFramebuffer::Create()
{
    ASSERT(!mFramebuffer.IsValid());
    ASSERT(mConfig.format != Format::Invalid);

    unsigned fbo_width = 0;
    unsigned fbo_height = 0;

    if (mConfig.format == Format::DepthTexture32f)
    {
        CreateDepthBufferTexture();

        auto* texture = GetDepthBufferTexture();
        fbo_width = texture->GetWidth();
        fbo_height = texture->GetHeight();
    }
    else
    {
        CreateColorBufferTextures();

        auto* texture = GetColorBufferTexture(0);
        fbo_width  = texture->GetWidth();
        fbo_height = texture->GetHeight();
    }
    dev::FramebufferConfig config;
    config.width  = fbo_width;
    config.height = fbo_height;
    config.msaa   = IsMultisampled();
    config.format = mConfig.format;
    mFramebuffer = mDevice->CreateFramebuffer(config);
    DEBUG("Created new frame buffer object. [name='%1', width=%2, height=%3, format=%4, samples=%5]",
          mName, fbo_width, fbo_height, mConfig.format, config.msaa);

    // commit the size
    mConfig.width  = fbo_width;
    mConfig.height = fbo_height;
    return true;
}

void DeviceFramebuffer::SetFrameStamp(size_t stamp)
{
    for (auto& texture : mColorTextures)
    {
        if (texture)
            texture->SetFrameStamp(stamp);
    }

    for (auto* texture : mClientColorTextures)
    {
        if (texture)
            texture->SetFrameStamp(stamp);
    }
    if (mClientDepthTexture)
        mClientDepthTexture->SetFrameStamp(stamp);

    mFrameNumber = stamp;
}

void DeviceFramebuffer::CreateColorBufferTextures()
{
    mClientColorTextures.resize(mConfig.color_target_count);
    mColorTextures.resize(mConfig.color_target_count);

    for (unsigned i=0; i<mConfig.color_target_count; ++i)
    {
        if (mClientColorTextures[i])
            continue;

        // we must have FBO width and height for creating
        // the color buffer texture.
        ASSERT(mConfig.width && mConfig.height);

        if (!mColorTextures[i])
        {
            const auto texture_name = "FBO/" + mName + "/Color" + std::to_string(i);
            mColorTextures[i] = std::make_unique<gfx::DeviceTexture>(mDevice, texture_name);
            mColorTextures[i]->SetName(texture_name);
            mColorTextures[i]->Allocate(mConfig.width, mConfig.height, gfx::Texture::Format::sRGBA);
            mColorTextures[i]->SetFilter(gfx::Texture::MinFilter::Linear);
            mColorTextures[i]->SetFilter(gfx::Texture::MagFilter::Linear);
            mColorTextures[i]->SetWrapX(gfx::Texture::Wrapping::Clamp);
            mColorTextures[i]->SetWrapY(gfx::Texture::Wrapping::Clamp);
            DEBUG("Allocated new FBO color buffer (texture) target. [name='%1', width=%2, height=%3]", mName,
                  mConfig.width, mConfig.height);
        }
        else
        {
            const auto width  = mColorTextures[i]->GetWidth();
            const auto height = mColorTextures[i]->GetHeight();
            if (width != mConfig.width || height != mConfig.height)
                mColorTextures[i]->Allocate(mConfig.width, mConfig.height, gfx::Texture::Format::sRGBA);
        }

    }
}

void DeviceFramebuffer::CreateDepthBufferTexture()
{
    if (mClientDepthTexture)
        return;

    ASSERT(mConfig.width && mConfig.height);

    if (!mDepthTexture)
    {
        const auto texture_name = "FBO/" + mName + "/DepthTexture";
        mDepthTexture = std::make_unique<gfx::DeviceTexture>(mDevice, texture_name);
        mDepthTexture->SetName(texture_name);
        mDepthTexture->Allocate(mConfig.width, mConfig.height, gfx::Texture::Format::DepthComponent32f);
        mDepthTexture->SetFilter(gfx::Texture::MinFilter::Nearest);
        mDepthTexture->SetFilter(gfx::Texture::MagFilter::Nearest);
        mDepthTexture->SetWrapX(gfx::Texture::Wrapping::Clamp);
        mDepthTexture->SetWrapY(gfx::Texture::Wrapping::Clamp);
        DEBUG("Allocated new FBO depth buffer (texture) target. [name='%1', width=%2, height=%3]", mName,
              mConfig.width, mConfig.height);
    }
    else
    {
        const auto width = mDepthTexture->GetWidth();
        const auto height = mDepthTexture->GetHeight();
        if (width != mConfig.width || height != mConfig.height)
            mDepthTexture->Allocate(mConfig.width, mConfig.height, Texture::Format::DepthComponent32f);
    }
}


} // namespace
