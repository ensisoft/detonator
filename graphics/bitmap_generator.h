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

#include <memory>

#include "data/fwd.h"
#include "graphics/types.h"

namespace gfx
{
    class IBitmap;

    // Interface for accessing / generating bitmaps procedurally.
    // Each implementation implements some procedural method for
    // creating a bitmap and generating its content.
    class IBitmapGenerator
    {
    public:
        // Get the function type of the implementation.
        enum class Function {
            Noise
        };
        // Destructor.
        virtual ~IBitmapGenerator() = default;
        // Get the generation function.
        virtual Function GetFunction() const = 0;
        // Generate a new bitmap using the algorithm.
        virtual std::unique_ptr<IBitmap> Generate() const = 0;
        // Clone this generator.
        virtual std::unique_ptr<IBitmapGenerator> Clone() const = 0;
        // Get the hash value of the generator.
        virtual std::size_t GetHash() const = 0;
        // Serialize the generator into JSON.
        virtual void IntoJson(data::Writer& data) const = 0;
        // Load the generator's state from the given JSON object.
        // Returns true if successful otherwise false.
        virtual bool FromJson(const data::Reader& data) = 0;
        // Get the width of the bitmaps that will be generated (in pixels).
        virtual unsigned GetWidth() const = 0;
        // Get the height of the bitmaps that will be generated (in pixels).
        virtual unsigned GetHeight() const = 0;
        // Set the width of the bitmaps that will be generated (in pixels).
        virtual void SetWidth(unsigned width) = 0;
        // Set the height of the bitmaps that will be generated (in pixels).
        virtual void SetHeight(unsigned height) = 0;
    private:
    };
} // namespace