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
        using Culling = gfx::Painter::Culling;
        using RenderPass = game::RenderPass;
        using Projection  = game::RenderProjection;

        enum class Flags {
            PP_Bloom
        };
        enum class Domain {
            Scene, Editor
        };
        enum class Source {
            Map, Scene
        };

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

    class BloomPass
    {
    public:
        BloomPass(const std::string& name, const gfx::Color4f& color, float threshold, const gfx::Painter& painter);

        void Draw(const SceneRenderLayerList& layers) const;
        std::string GetBloomTextureName() const;

        const gfx::Texture* GetBloomTexture() const;
    private:
        const std::string mName;
        const gfx::Color4f mColor;
        const float mThreshold = 1.0f;
        mutable gfx::Painter mPainter;
        mutable gfx::Texture* mBloomTexture = nullptr;
    };

    class MainRenderPass
    {
    public:
        MainRenderPass(gfx::Painter& painter)
          : mPainter(painter)
        {}
        void Draw(const SceneRenderLayerList& layers) const;
        void Composite(const BloomPass* bloom) const;
    private:
        gfx::Painter& mPainter;
    };

} // namespace
