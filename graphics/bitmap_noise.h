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

#pragma once

#include "config.h"

#include "graphics/bitmap_generator.h"

namespace gfx
{
    // Implement bitmap generation using a noise function.
    // Each layer of noise is defined as a function with prime
    // number seeds and frequency and amplitude. The bitmap
    // is filled by sampling all the noise layers / functions
    // and summing their signals.
    class NoiseBitmapGenerator : public IBitmapGenerator
    {
    public:
        struct Layer {
            std::uint32_t prime0 = 7;
            std::uint32_t prime1 = 743;
            std::uint32_t prime2 = 7873;
            float frequency = 1.0f;
            float amplitude = 1.0f;
        };
        NoiseBitmapGenerator() = default;
        NoiseBitmapGenerator(unsigned width, unsigned height)
            : mWidth(width)
            , mHeight(height)
        {}
        size_t GetNumLayers() const
        { return mLayers.size(); }
        void AddLayer(const Layer& layer)
        { mLayers.push_back(layer); }
        const Layer& GetLayer(size_t index) const
        { return mLayers[index]; }
        Layer& GetLayer(size_t index)
        { return mLayers[index]; }
        void DelLayer(size_t index)
        {
            auto it = mLayers.begin();
            std::advance(it, index);
            mLayers.erase(it);
        }
        bool HasLayers() const
        { return !mLayers.empty(); }
        // Create random generator settings.
        void Randomize(unsigned min_prime_index,
                       unsigned max_prime_index,
                       unsigned layers=1);

        Function GetFunction() const override
        { return Function::Noise; }
        std::unique_ptr<IBitmapGenerator> Clone() const override
        { return std::make_unique<NoiseBitmapGenerator>(*this); }
        unsigned GetWidth() const override
        { return mWidth; }
        unsigned GetHeight() const override
        { return mHeight; }
        void SetHeight(unsigned height) override
        { mHeight = height; }
        void SetWidth(unsigned width) override
        { mWidth = width; }
        void IntoJson(data::Writer& data) const override;
        bool FromJson(const data::Reader& data) override;
        std::unique_ptr<IBitmap> Generate() const override;
        std::size_t GetHash() const override;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        std::vector<Layer> mLayers;
    };
} // namespace gfx