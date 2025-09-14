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

#pragma once

#include "config.h"

#include <memory>

#include "graphics/material.h"
#include "graphics/text_buffer.h"
#include "graphics/types.h"

namespace gfx
{
    // material specialized for rendering text using
    // a pre-rasterized bitmap of text. Creates transient
    // texture objects for the text.
    class TextMaterial : public Material
    {
    public:
        explicit TextMaterial(const TextBuffer& text);
        explicit TextMaterial(TextBuffer&& text) noexcept;

        void SetFlag(Flags flag, bool on_off) noexcept override
        {
            if (on_off)
                mFlags |= static_cast<uint32_t>(flag);
            else mFlags &= ~static_cast<uint32_t>(flag);
        }

        bool TestFlag(Flags flag) const noexcept override
        {
            return (mFlags & static_cast<uint32_t>(flag)) != 0;
        }

        bool ApplyDynamicState(const Environment& env, Device& device, ProgramState& program, RasterState& raster) const override;
        void ApplyStaticState(const Environment& env, Device& device, ProgramState& program) const override;
        ShaderSource GetShader(const Environment& env, const Device& device) const override;
        std::string GetShaderId(const Environment&) const override;
        std::string GetShaderName(const Environment&) const override;

        void ComputeTextMetrics(unsigned* width, unsigned* height) const;

        void SetColor(const Color4f& color) noexcept
        { mColor = color; }
        // Set point sampling to true in order to use a fast filtering
        // when sampling from the texture. This should be for maximum perf
        // ideally when the geometry to be drawn matches closely with the
        // rasterized text texture/buffer. So when the texture maps onto
        // a rectangle and there's now transformation that would change the
        // rasterized dimensions (in pixels) of the rectangle from the
        // dimensions of the rasterized text texture.
        // The default is true.
        void SetPointSampling(bool on_off) noexcept
        { mPointSampling = on_off; }
    private:
        void InitDefaultFlags() noexcept;
    private:
        TextBuffer mText;
        Color4f mColor = Color::White;
        bool mPointSampling = true;
        std::int32_t mFlags = 0;
    };

    TextMaterial CreateMaterialFromText(const std::string& text,
                                        const std::string& font,
                                        const gfx::Color4f& color,
                                        unsigned font_size_px,
                                        unsigned raster_width,
                                        unsigned raster_height,
                                        unsigned text_align,
                                        unsigned text_prop,
                                        float line_height);


    std::unique_ptr<TextMaterial> CreateMaterialInstance(const TextBuffer& text);
    std::unique_ptr<TextMaterial> CreateMaterialInstance(TextBuffer&& text);
} // namespace