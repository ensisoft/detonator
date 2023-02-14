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

#include <vector>
#include <string>

#include "graphics/painter.h"
#include "graphics/fwd.h"

namespace engine
{
    struct RenderLayer {
        std::vector<gfx::Painter::DrawShape> draw_color_list;
        std::vector<gfx::Painter::DrawShape> mask_cover_list;
        std::vector<gfx::Painter::DrawShape> mask_expose_list;
    };

    using EntityRenderLayerList = std::vector<RenderLayer>;
    using SceneRenderLayerList  = std::vector<EntityRenderLayerList>;

    class BloomPass
    {
    public:
        BloomPass(const std::string& name, const gfx::Color4f& color, float threshold, gfx::Painter& painter);

        void Draw(const SceneRenderLayerList& layers) const;
        std::string GetBloomTextureName() const;

        const gfx::Texture* GetBloomTexture() const;
    private:
        const std::string mName;
        const gfx::Color4f mColor;
        const float mThreshold = 1.0f;
        gfx::Painter& mPainter;
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
