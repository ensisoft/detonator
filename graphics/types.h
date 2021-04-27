// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <algorithm> // for min, max
#include <optional>

#include "base/utility.h"
// simple type definitions

namespace gfx
{
    template<typename T>
    class Size
    {
    public:
        Size() = default;
        Size(T width, T height)
          : mWidth(width)
          , mHeight(height)
        {}
        template<typename F> explicit
        Size(const Size<F>& other)
          : mWidth(other.GetWidth())
          , mHeight(other.GetHeight())
        {}
        T GetWidth() const { return mWidth; }
        T GetHeight() const { return mHeight; }

        // returns true if either dimension is 0.
        bool IsZero() const
        {
            const bool has_width  = mWidth != T();
            const bool has_height = mHeight != T();
            if (has_height || has_width)
                return false;
            return true;
        }

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "w", mWidth);
            base::JsonWrite(json, "h", mHeight);
            return json;
        }
        static std::optional<Size<T>> FromJson(const nlohmann::json& object)
        {
            Size<T> ret;
            if (!base::JsonReadSafe(object, "w", &ret.mWidth) ||
                !base::JsonReadSafe(object, "h", &ret.mHeight))
                return std::nullopt;
            return ret;
        }

        std::size_t GetHash() const
        {
            std::size_t hash = 0;
            hash = base::hash_combine(hash, mWidth);
            hash = base::hash_combine(hash, mHeight);
            return hash;
        }
    private:
        T mWidth  = T();
        T mHeight = T();
    };
    template<typename T> inline
    Size<T> operator*(const Size<T>& size, T scale)
    {
        return Size<T>(size.GetWidth() * scale,
                       size.GetHeight() * scale);
    }

    using USize = Size<unsigned>;
    using FSize = Size<float>;
    using ISize = Size<int>;

    template<typename T>
    class Point
    {
    public:
        Point() = default;
        Point(T x, T y)
          : mX(x)
          , mY(y)
        {}
        template<typename F> explicit
        Point(const Point<F>& other)
          : mX(other.GetX())
          , mY(other.GetY())
        {}

        T GetX() const { return mX; }
        T GetY() const { return mY; }

        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "x", mX);
            base::JsonWrite(json, "y", mY);
            return json;
        }
        static std::optional<Point<T>> FromJson(const nlohmann::json& object)
        {
            Point<T> ret;
            if (!base::JsonReadSafe(object, "x", &ret.mX) ||
                !base::JsonReadSafe(object, "y", &ret.mY))
                return std::nullopt;
            return ret;
        }

        std::size_t GetHash() const
        {
            std::size_t hash = 0;
            hash = base::hash_combine(hash, mX);
            hash = base::hash_combine(hash, mY);
            return hash;
        }
    private:
        T mX = T();
        T mY = T();
    };

    template<typename T> inline
    Point<T> operator-(const Point<T>& lhs, const Point<T>& rhs)
    {
        return Point<T>(lhs.GetX() - rhs.GetX(),
                        lhs.GetY() - rhs.GetY());
    }
    template<typename T> inline
    Point<T> operator+(const Point<T>& lhs, const Point<T>& rhs)
    {
        return Point<T>(lhs.GetX() + rhs.GetX(),
                        lhs.GetY() + rhs.GetY());
    }

    using UPoint = Point<unsigned>;
    using FPoint = Point<float>;
    using IPoint = Point<int>;

    // simple rectangle definition
    template<typename T>
    class Rect
    {
    public:
        Rect() = default;
        Rect(T x, T y, T width, T height)
          : mX(x)
          , mY(y)
          , mWidth(width)
          , mHeight(height)
        {}
        template<typename F> explicit
        Rect(const Rect<F>& other)
        {
            mX = static_cast<T>(other.GetX());
            mY = static_cast<T>(other.GetY());
            mWidth = static_cast<T>(other.GetWidth());
            mHeight = static_cast<T>(other.GetHeight());
        }
        Rect(T x, T y, const Size<T>& size)
                : mX(x)
                , mY(y)
                , mWidth(size.GetWidth())
                , mHeight(size.GetHeight())
        {}
        Rect(const Point<T>& pos, T width, T height)
        {
            mX = pos.GetX();
            mY = pos.GetY();
            mWidth = width;
            mHeight = height;
        }
        Rect(const Point<T>& pos, const Size<T>& size)
        {
            mX = pos.GetX();
            mY = pos.GetY();
            mWidth  = size.GetWidth();
            mHeight = size.GetHeight();
        }

        T GetHeight() const
        { return mHeight; }
        T GetWidth() const
        { return mWidth; }
        T GetX() const
        { return mX; }
        T GetY() const
        { return mY; }
        void SetX(T value)
        { mX = value; }
        void SetY(T value)
        { mY = value; }
        void SetWidth(T width)
        { mWidth = width; }
        void SetHeight(T height)
        { mHeight = height; }
        void Resize(T width, T height)
        {
            mWidth = width;
            mHeight = height;
        }
        void Resize(const Size<T>& size)
        {
            Resize(size.GetWidth(), size.GetHeight());
        }
        void Grow(T some_width, T some_height)
        {
            mWidth += some_width;
            mHeight += some_height;
        }
        void Grow(const Size<T>& some_size)
        {
            Grow(some_size.GetWidth(), some_size.GetHeight());
        }
        void Move(T x, T y)
        {
            mX = x;
            mY = y;
        }
        void Move(const Point<T>& pos)
        {
            mX = pos.GetX();
            mY = pos.GetY();
        }
        void Translate(T x, T y)
        {
            mX += x;
            mY += y;
        }
        void Translate(const Point<T>& pos)
        {
            mX += pos.GetX();
            mY += pos.GetY();
        }
        bool IsEmpty() const
        {
            const bool has_width  = mWidth != T();
            const bool has_height = mHeight != T();
            return !has_width || !has_height;
        }

        // Map a local point relative to the rect origin
        // into a global point relative to the origin of the
        // coordinate system.
        Point<T> MapToGlobal(T x, T y) const
        {
            return Point<T>(mX + x, mY + y);
        }

        // Map a local point relative to the rect origin
        // into a global point relative to the origin of the
        // coordinate system
        Point<T> MapToGlobal(const Point<T>& p) const
        {
            return Point<T>(mX + p.GetX(), mY + p.GetY());
        }

        // Map a global point relative to the origin of the
        // coordinate system to a local point relative to the
        // origin of the rect.
        Point<T> MapToLocal(T x, T y) const
        {
            return Point<T>(x - mX, y - mY);
        }
        // Map a global point relative to the origin of the
        // coordinate system to a local point relative to the
        // origin of the rect.
        Point<T> MapToLocal(const Point<T>& p) const
        {
            return Point<T>(p.GetX() - mX, p.GetY() - mY);
        }

        // Normalize the rectangle with respect to the given
        // dimensions.
        Rect<float> Normalize(const Size<float>& space) const
        {
            const float x = mX / space.GetWidth();
            const float y = mY / space.GetHeight();
            const float w = mWidth / space.GetWidth();
            const float h = mHeight / space.GetHeight();
            return Rect<float>(x, y, w, h);
        }

        // Inverse of Normalize. Assuming a normalized rectangle
        // expand it to a non-normalized rectangle given the
        // dimensions.
        template<typename F>
        Rect<F> Expand(const Size<F>& space) const
        {
            // floats or ints.
            const F x = mX * space.GetWidth();
            const F y = mY * space.GetHeight();
            const F w = mWidth * space.GetWidth();
            const F h = mHeight * space.GetHeight();
            return Rect<F>(x, y, w, h);
        }
        // Inverse of Normalize. Assuming a normalized rectangle
        // expand it to a non-normalized rectangle given the
        // dimensions. Since the return rect is expressed with
        // unsigned types the rectangle cannot have a negative
        // x,y coord and such negative coords will be clamped to 0, 0
        Rect<unsigned> Expand(const Size<unsigned>& space) const
        {
            // float to unsigned can be undefined conversion if the
            // value cannot be represented in the destination type.
            // i.e. negative floats cannot be represented in unsigned type.
            // http://eel.is/c++draft/conv#fpint
            const unsigned x = std::max(mX, T(0)) * space.GetWidth();
            const unsigned y = std::max(mY, T(0)) * space.GetHeight();
            const unsigned w = mWidth * space.GetWidth();
            const unsigned h = mHeight * space.GetHeight();
            return Rect<unsigned>(x, y, w, h);
        }

        // Serialize into JSON
        nlohmann::json ToJson() const
        {
            nlohmann::json json;
            base::JsonWrite(json, "x", mX);
            base::JsonWrite(json, "y", mY);
            base::JsonWrite(json, "w", mWidth);
            base::JsonWrite(json, "h", mHeight);
            return json;
        }

        // Create Rect from JSON and indicate through success whether
        // the JSON contains all the required data or not.
        static std::optional<Rect<T>> FromJson(const nlohmann::json& object)
        {
            Rect<T> ret;
            if (!base::JsonReadSafe(object, "x", &ret.mX) ||
                !base::JsonReadSafe(object, "y", &ret.mY) ||
                !base::JsonReadSafe(object, "w", &ret.mWidth) ||
                !base::JsonReadSafe(object, "h", &ret.mHeight))
                return std::nullopt;

            return ret;
        }

        std::size_t GetHash() const
        {
            std::size_t hash = 0;
            hash = base::hash_combine(hash, mX);
            hash = base::hash_combine(hash, mY);
            hash = base::hash_combine(hash, mWidth);
            hash = base::hash_combine(hash, mHeight);
            return hash;
        }

    private:
        T mX = 0;
        T mY = 0;
        T mWidth = 0;
        T mHeight = 0;
    };

    using URect = Rect<unsigned>;
    using FRect = Rect<float>;
    using IRect = Rect<int>;

    // Find the intersection of the two rectangles
    // and return the rectangle that represents the intersect.
    template<typename T>
    Rect<T> Intersect(const Rect<T>& lhs, const Rect<T>& rhs)
    {
        using R = Rect<T>;
        if (lhs.IsEmpty() || rhs.IsEmpty())
            return R();

        const auto lhs_top    = lhs.GetY();
        const auto lhs_left   = lhs.GetX();
        const auto lhs_right  = lhs_left + lhs.GetWidth();
        const auto lhs_bottom = lhs_top + lhs.GetHeight();
        const auto rhs_top    = rhs.GetY();
        const auto rhs_left   = rhs.GetX();
        const auto rhs_right  = rhs_left + rhs.GetWidth();
        const auto rhs_bottom = rhs_top + rhs.GetHeight();
        // no intersection conditions
        if (lhs_right <= rhs_left)
            return R();
        else if (lhs_left >= rhs_right)
            return R();
        else if (lhs_top >= rhs_bottom)
            return R();
        else if (lhs_bottom <= rhs_top)
            return R();

        const auto right  = std::min(lhs_right, rhs_right);
        const auto left   = std::max(lhs_left, rhs_left);
        const auto top    = std::max(lhs_top, rhs_top);
        const auto bottom = std::min(lhs_bottom, rhs_bottom);
        return R(left, top, right - left, bottom - top);
    }

    template<typename T>
    bool DoesIntersect(const Rect<T>& lhs, const Rect<T>& rhs)
    {
        using R = Rect<T>;
        if (lhs.IsEmpty() || rhs.IsEmpty())
            return false;

        const auto lhs_top    = lhs.GetY();
        const auto lhs_left   = lhs.GetX();
        const auto lhs_right  = lhs_left + lhs.GetWidth();
        const auto lhs_bottom = lhs_top + lhs.GetHeight();
        const auto rhs_top    = rhs.GetY();
        const auto rhs_left   = rhs.GetX();
        const auto rhs_right  = rhs_left + rhs.GetWidth();
        const auto rhs_bottom = rhs_top + rhs.GetHeight();
        // no intersection conditions
        if ((lhs_right <= rhs_left) ||
            (lhs_left >= rhs_right) ||
            (lhs_top >= rhs_bottom) ||
            (lhs_bottom <= rhs_top))
            return false;
        return true;
    }

    template<typename T>
    Rect<T> Union(const Rect<T>& lhs, const Rect<T>& rhs)
    {
        using R = Rect<T>;

        const auto lhs_top    = lhs.GetY();
        const auto lhs_left   = lhs.GetX();
        const auto lhs_right  = lhs_left + lhs.GetWidth();
        const auto lhs_bottom = lhs_top + lhs.GetHeight();
        const auto rhs_top    = rhs.GetY();
        const auto rhs_left   = rhs.GetX();
        const auto rhs_right  = rhs_left + rhs.GetWidth();
        const auto rhs_bottom = rhs_top + rhs.GetHeight();

        const auto left = std::min(lhs_left, rhs_left);
        const auto right = std::max(lhs_right, rhs_right);
        const auto top = std::min(lhs_top, rhs_top);
        const auto bottom = std::max(lhs_bottom, rhs_bottom);
        return R(left, top, right - left, bottom - top);
    }

} // namespace
