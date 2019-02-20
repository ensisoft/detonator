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

namespace invaders
{
    class Texture
    {
    public:
        virtual ~Texture() = default;

        enum class Format {
            RGB, RGBA
        };
        enum class MinFilter {
            Nearest, Linear, Mipmap
        };
        enum class MagFilter {
            Nearest, Linear
        };

        // default filtering.
        virtual void SetFilter(MinFilter filter) = 0;
        virtual void SetFilter(MagFilter filter) = 0;

        virtual MinFilter GetMinFilter() const = 0;
        virtual MagFilter GetMagFilter() const = 0;

        // upload the texture contents from the given buffer.
        virtual void Upload(const void* bytes,
            unsigned xres, unsigned yres, Format format) = 0;

        // upload the texture contents from the given file.
        virtual void UploadFromFile(const std::string& filename) = 0;

        virtual unsigned GetWidth() const = 0;
        virtual unsigned GetHeight() const = 0;

    protected:
    private:
    };


} // namespace