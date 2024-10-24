// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include <glm/mat4x4.hpp>
#include "warnpop.h"

#include <vector>
#include <string>
#include <memory>

#include "base/bitflag.h"
#include "graphics/painter.h"
#include "graphics/fwd.h"
#include "game/enum.h"

namespace engine
{
    struct DrawPacket {
        using DepthTest  = gfx::Painter::DepthTest;
        using Culling    = gfx::Painter::Culling;
        using RenderPass = game::RenderPass;
        using Projection = game::RenderProjection;

        enum class Flags {
            PP_Bloom, CullPacket
        };
        enum class Domain {
            Scene, Editor
        };
        enum class Source {
            Map, Scene
        };

        DepthTest depth_test = DepthTest::Disabled;

        RenderPass pass = RenderPass::DrawColor;
        Projection projection = Projection::Orthographic;

        Culling culling = Culling::Back;

        Source source = Source::Scene;

        Domain domain = Domain::Scene;
        // flags to control the rendering etc.
        base::bitflag<Flags> flags;
        // shortcut to the node's material.
        std::shared_ptr<const gfx::Material> material;
        // shortcut to the node's drawable.
        std::shared_ptr<const gfx::Drawable> drawable;
        // model to world transform for transforming the drawable.
        glm::mat4 transform;
        // the sort point (in model space) for mapping a point from the
        // model into tile map coordinates. y = 0.0f = top, y = 1.0 = bottom.
        // there's no clamping so negative values and range beyond 0.0f - 1.0f
        // is allowed.
        glm::vec2 sort_point = {0.5f, 1.0f};

        // render_layer and packet index together define the order of packets
        // when sorting for rendering.
        // render_layer is the 1st order sorting key followed by packet_index.
        // in other words
        // 0. = render_layer=0, packet_index=0,
        // 1. = render_layer=0, packet_index=1
        // 2. = render_layer=1, packet_index=0
        // etc.
        std::int32_t render_layer = 0;
        // packet index wthhin the render layer.
        std::int32_t packet_index = 0;

        // these values are only used / valid when the packet has been
        // created in conjunction with tilemap.

        // the row on the map as mapped based on the sort point on the model
        std::uint32_t map_row = 0;
        // the column on the map as mapped based on the sort point on the model.
        std::uint32_t map_col = 0;
        // map layer.
        std::uint16_t map_layer = 0;

        float line_width = 1.0f;
    };

    struct RenderLayer {
        std::vector<gfx::Painter::DrawCommand> draw_color_list;
        std::vector<gfx::Painter::DrawCommand> mask_cover_list;
        std::vector<gfx::Painter::DrawCommand> mask_expose_list;
    };

    using EntityRenderLayerList = std::vector<RenderLayer>;
    using SceneRenderLayerList  = std::vector<EntityRenderLayerList>;

    class LowLevelRendererHook
    {
    public:
        struct RenderSettings {
            bool editing_mode = false;
            bool enable_bloom = false;
            float bloom_threshold = 0.0f;
            gfx::Color4f bloom_color;
            gfx::Color4f clear_color;
            gfx::USize surface_size;
            gfx::IRect viewport;
            glm::vec2 pixel_ratio = {1.0f, 1.0f};
        };

        struct GPUResources {
            gfx::Device* device = nullptr;
            gfx::Framebuffer* framebuffer = nullptr;
            gfx::Texture* main_image = nullptr;
        };

        virtual void BeginDraw(const RenderSettings& settings, const GPUResources& gpu) {}
        virtual void EndDraw(const RenderSettings& settings, const GPUResources& gpu) {}
    };

    class LowLevelRenderer
    {
    public:
        using RenderSettings = LowLevelRendererHook::RenderSettings;

        LowLevelRenderer(const std::string* name, gfx::Device& device);

        inline void SetSurfaceSize(const gfx::USize& size) noexcept
        {
            mSettings.surface_size = size;
        }
        inline void SetPixelRatio(const glm::vec2& ratio) noexcept
        {
            mSettings.pixel_ratio = ratio;
        }
        inline void SetViewport(const gfx::IRect& viewport) noexcept
        {
            mSettings.viewport = viewport;
        }

        inline void SetEditingMode(bool on_off) noexcept
        {
            mSettings.editing_mode = on_off;
        }

        inline void EnableBloom(bool on_off) noexcept
        {
            mSettings.enable_bloom = on_off;
        }
        inline void SetBloomParams(const gfx::Color4f& color, float threshold) noexcept
        {
            mSettings.bloom_color = color;
            mSettings.bloom_threshold = threshold;
        }
        inline void SetClearColor(const gfx::Color4f& color) noexcept
        {
            mSettings.clear_color = color;
        }

        inline void SetRenderHook(LowLevelRendererHook* hook) noexcept
        {
            mRenderHook = hook;
        }

        void Draw(const SceneRenderLayerList& layers) const;
        void Blit() const;

    private:
        unsigned GetSurfaceWidth() const noexcept
        {
            return mSettings.surface_size.GetWidth();
        }
        unsigned GetSurfaceHeight() const noexcept
        {
            return mSettings.surface_size.GetHeight();
        }

    private:
        gfx::Texture* CreateTextureTarget(const std::string& name) const;
        gfx::Framebuffer* CreateFrameBuffer(const std::string& name) const;

    private:
        const std::string* mRendererName = nullptr;
        LowLevelRendererHook* mRenderHook = nullptr;
        RenderSettings mSettings;
        mutable gfx::Texture* mMainImage = nullptr;
        mutable gfx::Texture* mBloomImage = nullptr;
        mutable gfx::Framebuffer* mMainFBO = nullptr;
        gfx::Device& mDevice;
    };

} // namespace
