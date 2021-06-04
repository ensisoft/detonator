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

#include <algorithm> // for min, max

namespace base
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
    template<typename T> inline
    Size<T> operator+(const Size<T>& rhs, const Size<T>& lhs)
    {
        return Size<T>(rhs.GetWidth() + lhs.GetWidth(),
                       rhs.GetHeight() + lhs.GetHeight());
    }
    template<typename T> inline
    Size<T> operator-(const Size<T>& rhs, const Size<T>& lhs)
    {
        return Size<T>(rhs.GetWidth() - lhs.GetWidth(),
                       rhs.GetHeight() - lhs.GetHeight());
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

        Point& operator+=(const Point& other)
        {
            mX += other.mX;
            mY += other.mY;
            return *this;
        }
        Point& operator-=(const Point& other)
        {
            mX -= other.mX;
            mY -= other.mY;
            return *this;
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
        Point<T> GetPosition() const
        { return Point<T>(mX, mY); }
        Size<T> GetSize() const
        { return Size<T>(mWidth, mHeight); }
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

        // Return true if the point p is within this rectangle.
        bool TestPoint(const Point<T>& p) const
        {
            if (p.GetX() <= mX || p.GetY() <= mY)
                return false;
            else if (p.GetX() >= mX + mWidth || p.GetY() >= mY + mHeight)
                return false;
            return true;
        }
        inline bool TestPoint(T x, T y) const
        { return TestPoint(Point<T>(x, y)); }

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
