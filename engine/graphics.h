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
#include "graphics/types.h"
#include "graphics/painter.h"
#include "graphics/fwd.h"
#include "engine/camera.h"
#include "engine/types.h"
#include "engine/color.h"
#include "game/enum.h"
#include "game/types.h"

namespace engine
{
    struct DrawPacket {
        using DepthTest  = gfx::Painter::DepthTest;
        using Culling    = gfx::Painter::Culling;
        using RenderPass = game::RenderPass;
        using Projection = game::RenderProjection;
        using CoordinateSpace = game::CoordinateSpace;

        enum class Flags {
            PP_Bloom, CullPacket,
            Flip_UV_Vertically, Flip_UV_Horizontally
        };
        enum class Domain {
            Scene, Editor
        };
        enum class Source {
            // warning do not change the order of these.
            // the value is used directly when sorting tilemap tiles.
            Map, Scene
        };

        DepthTest depth_test = DepthTest::Disabled;

        RenderPass pass = RenderPass::DrawColor;
        Projection projection = Projection::Orthographic;
        CoordinateSpace coordinate_space = CoordinateSpace::Scene;

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

        std::uint8_t map_sort_key = 0;

        float line_width = 1.0f;
    };

    struct Light {
        std::shared_ptr<const gfx::BasicLight> light;

        glm::vec2 sort_point = {0.0f, 0.0f};

        // model to world transform for transforming the light
        // to world coordinates.
        glm::mat4 transform;

        std::int32_t render_layer = 0;
        std::int32_t packet_index = 0;

        std::uint32_t map_row = 0;
        std::uint32_t map_col = 0;
        std::uint16_t map_layer = 0;
    };

    class PacketFilter
    {
    public:
        virtual ~PacketFilter() = default;
        virtual bool InspectPacket(DrawPacket& packet) { return true; }
    };

    struct RenderLayer {
        std::vector<gfx::Painter::DrawCommand> draw_color_list;
        std::vector<gfx::Painter::DrawCommand> mask_cover_list;
        std::vector<gfx::Painter::DrawCommand> mask_expose_list;
        std::vector<const Light*> layer_lights;
    };

    using EntityRenderLayerList = std::vector<RenderLayer>;
    using SceneRenderLayerList  = std::vector<EntityRenderLayerList>;
    using DrawPacketList = std::vector<DrawPacket>;
    using LightList = std::vector<Light>;

    class LowLevelRendererHook
    {
    public:
        struct BloomParams {
            float threshold = 0.0f;
            float red   = 0.0f;
            float green = 0.0f;
            float blue  = 0.0f;
        };
        using BasicFogMode   = game::BasicFogMode;
        using BasicFogParams = game::BasicFogParameters;

        struct Camera {
            Color4f clear_color;
            glm::vec2 position = {0.0f, 0.0f};
            glm::vec2 scale    = {1.0f, 1.0f};
            FRect viewport;
            float rotation = 0.0f;

            PerspectiveProjectionArgs ppa;

            GameView map_perspective;
        };

        // The rendering window/surface details.
        struct Surface {
            // Device viewport in which part of the surface to render.
            IRect viewport;
            // Rendering surface size in pixels.
            USize size;
        };

        struct RenderSettings {
            Camera camera;
            Surface surface;
            bool editing_mode = false;
            bool enable_bloom = false;
            bool enable_lights = false;
            bool enable_fog    = false;
            glm::vec2 pixel_ratio = {1.0f, 1.0f};
            BloomParams bloom;
            BasicFogParams fog;
        };

        struct GPUResources {
            gfx::Device* device = nullptr;
            gfx::Framebuffer* framebuffer = nullptr;
            gfx::Texture* main_image = nullptr;
        };

        virtual void BeginDraw(const RenderSettings& settings, const GPUResources& gpu) {}
        virtual void EndDraw(const RenderSettings& settings, const GPUResources& gpu) {}
        virtual void EndDrawPacket(const RenderSettings& settings, const GPUResources& gpu, const DrawPacket& packet, gfx::Painter& painter) {}
    };

    class LowLevelRenderer
    {
    public:
        using BloomParams = LowLevelRendererHook::BloomParams;
        using Camera = LowLevelRendererHook::Camera;
        using Surface = LowLevelRendererHook::Surface;
        using RenderSettings = LowLevelRendererHook::RenderSettings;
        using BasicFogParams = LowLevelRendererHook::BasicFogParams;

        LowLevelRenderer(const std::string* name, gfx::Device& device);

        void SetBloom(const BloomParams& bloom) noexcept
        {
            mSettings.bloom = bloom;
        }
        void SetFog(const BasicFogParams& fog) noexcept
        {
            mSettings.fog = fog;
        }
        void SetCamera(const Camera& camera) noexcept
        {
            mSettings.camera = camera;
        }
        void SetSurface(const Surface& surface) noexcept
        {
            mSettings.surface = surface;
        }

        void SetPixelRatio(const glm::vec2& ratio) noexcept
        {
            mSettings.pixel_ratio = ratio;
        }
        void SetEditingMode(bool on_off) noexcept
        {
            mSettings.editing_mode = on_off;
        }

        void EnableLights(bool on_off) noexcept
        {
            mSettings.enable_lights = on_off;
        }
        void EnableFog(bool on_off) noexcept
        {
            mSettings.enable_fog = on_off;
        }

        void EnableBloom(bool on_off) noexcept
        {
            mSettings.enable_bloom = on_off;
        }
        void SetRenderHook(LowLevelRendererHook* hook) noexcept
        {
            mRenderHook = hook;
        }
        void SetPacketFilter(PacketFilter* packet_filter) noexcept
        {
            mPacketFilter = packet_filter;
        }

        void DrawPackets(DrawPacketList& packets, LightList& lights) const;
        void BlitImage() const;

    private:
        unsigned GetSurfaceWidth() const noexcept
        {
            return mSettings.surface.size.GetWidth();
        }
        unsigned GetSurfaceHeight() const noexcept
        {
            return mSettings.surface.size.GetHeight();
        }
        void ConfigureProgram(gfx::GenericShaderProgram& program) const;

        void DrawDefault(DrawPacketList& packets, LightList& lights) const;
        void DrawFramebuffer(DrawPacketList& packets, LightList& lights) const;
        void Draw(DrawPacketList& packets, LightList& lights, gfx::Framebuffer* fbo, gfx::GenericShaderProgram& program) const;
        bool CullDrawPacket(const DrawPacket& packet, const glm::mat4& projection, const glm::mat4& modelview) const;

    private:
        gfx::Texture* CreateTextureTarget(const std::string& name) const;
        gfx::Framebuffer* CreateFrameBuffer(const std::string& name) const;

    private:
        const std::string* mRendererName = nullptr;
        LowLevelRendererHook* mRenderHook = nullptr;
        PacketFilter* mPacketFilter = nullptr;
        RenderSettings mSettings;
        mutable gfx::Texture* mMainImage = nullptr;
        mutable gfx::Texture* mBloomImage = nullptr;
        mutable gfx::Framebuffer* mMainFBO = nullptr;
        gfx::Device& mDevice;
    };

} // namespace
