
// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#pragma once

#include "config.h"
#include "base/assert.h"

namespace gfx
{
    class Texture
    {
    public:
        virtual ~Texture() = default;

        enum class Format {
            RGB, RGBA, Grayscale
        };
        // Texture minifying filter is used whenever the 
        // pixel being textured maps to an area greater than
        // one texture element. 
        enum class MinFilter {
            // Use the texture element nearest to the 
            // center of the pixel (Manhattan distance)
            Nearest,
            // Use the weighted average of the four texture
            // elements that are closest to the pixel.
            Linear, 
            // Use mips (precomputed) minified textures.
            // Use the nearest texture element from the nearest 
            // mipmap level
            Mipmap,
            // Use mips (precomputed) minified textures.
            // Use the weighted average of the four texture
            // elements that are sampled from the closest mipmap level.
            Bilinear, 
            // Use mips (precomputd minified textures.
            // Use the weigted average of the four texture
            // elements that are sampled from the two nearest mipmap levels.
            Trilinear
        };
        
        // Texture magnifying filter is used whenver the 
        // pixel being textured maps to to an area less than 
        // one texture element.
        enum class MagFilter {
            // Use the texture element nearest to the center
            // of the pixel. (Manhattan distance).
            Nearest, 
            // Use the weighted average of the four texture
            // elements that are closest to the pixel.
            Linear
        };

        // Identify texture format based on the bit depth
        static Format DepthToFormat(unsigned bit_depth)
        {
            if (bit_depth == 8)
                return Format::Grayscale;
            else if (bit_depth == 24)
                return Format::RGB; 
            else if (bit_depth == 32)
                return Format::RGBA;
            // this function is only valid for the above bit depths.
            // everything else is considered a bug.
            // when reading data from external sources validation
            // of expected formats needs to be done elsewhere.
            ASSERT(!"Unexpected bit depth detected.");
        }

        // Set texture minification filter.
        virtual void SetFilter(MinFilter filter) = 0;
        // Set texture magnification filter.
        virtual void SetFilter(MagFilter filter) = 0;

        virtual MinFilter GetMinFilter() const = 0;
        virtual MagFilter GetMagFilter() const = 0;

        // upload the texture contents from the given buffer.
        virtual void Upload(const void* bytes,
            unsigned xres, unsigned yres, Format format) = 0;

        virtual unsigned GetWidth() const = 0;
        virtual unsigned GetHeight() const = 0;
        virtual Format GetFormat() const = 0;

        // Enable or disable this texture from being garbage collected.
        // When this is enabled the texture may be deleted if it goes
        // unused long enough.
        // The default state is false. I.e. no garbage collection.
        virtual void EnableGarbageCollection(bool gc) = 0;

    protected:
    private:
    };


} // namespace