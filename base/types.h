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
#include <tuple>

#include "base/math.h"

namespace base
{
    template<typename T>
    class Size
    {
    public:
        Size() = default;
        Size(T width, T height) noexcept
          : mWidth(width)
          , mHeight(height)
        {}
        template<typename F> explicit
        Size(const Size<F>& other) noexcept
          : mWidth(other.GetWidth())
          , mHeight(other.GetHeight())
        {}
        T GetWidth() const noexcept { return mWidth; }
        T GetHeight() const noexcept { return mHeight; }

        // returns true if either dimension is 0.
        bool IsZero() const noexcept
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
    Size<T> operator*(const Size<T>& size, T scale) noexcept
    {
        return Size<T>(size.GetWidth() * scale,
                       size.GetHeight() * scale);
    }
    template<typename T> inline
    Size<T> operator+(const Size<T>& rhs, const Size<T>& lhs) noexcept
    {
        return Size<T>(rhs.GetWidth() + lhs.GetWidth(),
                       rhs.GetHeight() + lhs.GetHeight());
    }
    template<typename T> inline
    Size<T> operator-(const Size<T>& rhs, const Size<T>& lhs) noexcept
    {
        return Size<T>(rhs.GetWidth() - lhs.GetWidth(),
                       rhs.GetHeight() - lhs.GetHeight());
    }

    using USize = Size<unsigned>;
    using FSize = Size<float>;
    using ISize = Size<int>;

    inline bool operator==(const USize& lhs, const USize& rhs) noexcept
    {
        return lhs.GetWidth() == rhs.GetWidth() &&
               lhs.GetHeight() == rhs.GetHeight();
    }
    inline bool operator==(const ISize& lhs, const ISize& rhs) noexcept
    {
        return lhs.GetWidth() == rhs.GetWidth() &&
               lhs.GetHeight() == rhs.GetHeight();
    }
    inline bool operator!=(const USize& lhs, const USize& rhs) noexcept
    {
        return !(lhs == rhs);
    }
    inline bool operator!=(const ISize& lhs, const ISize& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template<typename T>
    class Point
    {
    public:
        Point() = default;
        Point(T x, T y) noexcept
          : mX(x)
          , mY(y)
        {}
        template<typename F> explicit
        Point(const Point<F>& other) noexcept
          : mX(other.GetX())
          , mY(other.GetY())
        {}

        T GetX() const noexcept { return mX; }
        T GetY() const noexcept { return mY; }

        Point& operator+=(const Point& other) noexcept
        {
            mX += other.mX;
            mY += other.mY;
            return *this;
        }
        Point& operator-=(const Point& other) noexcept
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
    Point<T> operator-(const Point<T>& lhs, const Point<T>& rhs) noexcept
    {
        return Point<T>(lhs.GetX() - rhs.GetX(),
                        lhs.GetY() - rhs.GetY());
    }
    template<typename T> inline
    Point<T> operator+(const Point<T>& lhs, const Point<T>& rhs) noexcept
    {
        return Point<T>(lhs.GetX() + rhs.GetX(),
                        lhs.GetY() + rhs.GetY());
    }

    using UPoint = Point<unsigned>;
    using FPoint = Point<float>;
    using IPoint = Point<int>;

    inline bool operator==(const UPoint& lhs, const UPoint& rhs) noexcept
    {
        return lhs.GetX() == rhs.GetX() &&
               lhs.GetY() == rhs.GetY();
    }
    inline bool operator==(const IPoint& lhs, const IPoint& rhs) noexcept
    {
        return lhs.GetX() == rhs.GetX() &&
               lhs.GetY() == rhs.GetY();
    }
    inline bool operator!=(const UPoint& lhs, const UPoint& rhs) noexcept
    {
        return !(lhs == rhs);
    }
    inline bool operator!=(const IPoint& lhs, const IPoint& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    // simple rectangle definition
    template<typename T>
    class Rect
    {
    public:
        using PointT = Point<T>;
        using SizeT  = Size<T>;

        Rect() = default;
        Rect(T x, T y, T width, T height) noexcept
          : mX(x)
          , mY(y)
          , mWidth(width)
          , mHeight(height)
        {}
        template<typename F> explicit
        Rect(const Rect<F>& other) noexcept
        {
            mX = static_cast<T>(other.GetX());
            mY = static_cast<T>(other.GetY());
            mWidth = static_cast<T>(other.GetWidth());
            mHeight = static_cast<T>(other.GetHeight());
        }
        Rect(T x, T y, const Size<T>& size) noexcept
          : mX(x)
          , mY(y)
          , mWidth(size.GetWidth())
          , mHeight(size.GetHeight())
        {}
        Rect(const Point<T>& pos, T width, T height) noexcept
        {
            mX = pos.GetX();
            mY = pos.GetY();
            mWidth = width;
            mHeight = height;
        }
        Rect(const Point<T>& pos, const Size<T>& size) noexcept
        {
            mX = pos.GetX();
            mY = pos.GetY();
            mWidth  = size.GetWidth();
            mHeight = size.GetHeight();
        }

        T GetHeight() const noexcept
        { return mHeight; }
        T GetWidth() const noexcept
        { return mWidth; }
        T GetX() const noexcept
        { return mX; }
        T GetY() const noexcept
        { return mY; }
        Point<T> GetPosition() const noexcept
        { return {mX, mY}; }
        Size<T> GetSize() const noexcept
        { return {mWidth, mHeight}; }
        void SetX(T value) noexcept
        { mX = value; }
        void SetY(T value) noexcept
        { mY = value; }
        void SetWidth(T width) noexcept
        { mWidth = width; }
        void SetHeight(T height) noexcept
        { mHeight = height; }
        void Resize(T width, T height) noexcept
        {
            mWidth = width;
            mHeight = height;
        }
        void Resize(const Size<T>& size) noexcept
        {
            Resize(size.GetWidth(), size.GetHeight());
        }
        void Grow(T some_width, T some_height) noexcept
        {
            mWidth += some_width;
            mHeight += some_height;
        }
        void Grow(const Size<T>& some_size) noexcept
        {
            Grow(some_size.GetWidth(), some_size.GetHeight());
        }
        void Move(T x, T y) noexcept
        {
            mX = x;
            mY = y;
        }
        void Move(const Point<T>& pos) noexcept
        {
            mX = pos.GetX();
            mY = pos.GetY();
        }
        void Translate(T x, T y) noexcept
        {
            mX += x;
            mY += y;
        }
        void Translate(const Point<T>& pos) noexcept
        {
            mX += pos.GetX();
            mY += pos.GetY();
        }
        bool IsEmpty() const noexcept
        {
            const bool has_width  = mWidth != T();
            const bool has_height = mHeight != T();
            return !has_width || !has_height;
        }

        // Return true if the point p is within this rectangle.
        bool TestPoint(const Point<T>& p) const noexcept
        {
            if (p.GetX() < mX || p.GetY() < mY)
                return false;
            else if (p.GetX() > mX + mWidth || p.GetY() > mY + mHeight)
                return false;
            return true;
        }
        bool TestPoint(T x, T y) const noexcept
        {
            return TestPoint(Point<T>(x, y));
        }

        // Map a local point relative to the rect origin
        // into a global point relative to the origin of the
        // coordinate system.
        Point<T> MapToGlobal(T x, T y) const noexcept
        {
            return { mX + x, mY + y };
        }

        // Map a local point relative to the rect origin
        // into a global point relative to the origin of the
        // coordinate system
        Point<T> MapToGlobal(const Point<T>& p) const noexcept
        {
            return { mX + p.GetX(), mY + p.GetY() };
        }

        // Map a global point relative to the origin of the
        // coordinate system to a local point relative to the
        // origin of the rect.
        Point<T> MapToLocal(T x, T y) const noexcept
        {
            return { x - mX, y - mY };
        }
        // Map a global point relative to the origin of the
        // coordinate system to a local point relative to the
        // origin of the rect.
        Point<T> MapToLocal(const Point<T>& p) const noexcept
        {
            return { p.GetX() - mX, p.GetY() - mY };
        }

        // Normalize the rectangle with respect to the given
        // dimensions.
        Rect<float> Normalize(const Size<float>& space) const noexcept
        {
            const float x = mX / space.GetWidth();
            const float y = mY / space.GetHeight();
            const float w = mWidth / space.GetWidth();
            const float h = mHeight / space.GetHeight();
            return {x, y, w, h};
        }

        // Inverse of Normalize. Assuming a normalized rectangle
        // expand it to a non-normalized rectangle given the dimensions.
        template<typename F>
        Rect<F> Expand(const Size<F>& space) const noexcept
        {
            // floats or ints.
            const F x = mX * space.GetWidth();
            const F y = mY * space.GetHeight();
            const F w = mWidth * space.GetWidth();
            const F h = mHeight * space.GetHeight();
            return {x, y, w, h};
        }
        // Inverse of Normalize. Assuming a normalized rectangle
        // expand it to a non-normalized rectangle given the
        // dimensions. Since the return rect is expressed with
        // unsigned types the rectangle cannot have a negative
        // x,y coord and such negative coords will be clamped to 0, 0
        Rect<unsigned> Expand(const Size<unsigned>& space) const noexcept
        {
            // float to unsigned can be undefined conversion if the
            // value cannot be represented in the destination type.
            // i.e. negative floats cannot be represented in unsigned type.
            // http://eel.is/c++draft/conv#fpint
            const unsigned x = std::max(mX, T(0)) * space.GetWidth();
            const unsigned y = std::max(mY, T(0)) * space.GetHeight();
            const unsigned w = mWidth * space.GetWidth();
            const unsigned h = mHeight * space.GetHeight();
            return {x, y, w, h};
        }
        // Split the rect into 4 sub-quadrants.
        std::tuple<Rect, Rect, Rect, Rect> GetQuadrants() const noexcept
        {
            const auto half = T(2);
            const auto half_width = mWidth/half;
            const auto half_height = mHeight/half;
            const Rect r0(mX, mY, half_width, half_height);
            const Rect r1(mX, mY+half_height, half_width, half_height);
            const Rect r2(mX+half_width, mY, half_width, half_height);
            const Rect r3(mX+half_width, mY+half_height, half_width, half_height);
            return {r0, r1, r2, r3};
        }
        // Get the 4 corners of the rectangle.
        std::tuple<PointT, PointT, PointT, PointT> GetCorners() const noexcept
        {
            const PointT p0(mX, mY);
            const PointT p1(mX, mY+mHeight);
            const PointT p2(mX+mWidth, mY);
            const PointT p3(mX+mWidth, mY+mHeight);
            return {p0, p1, p2, p3};
        }
        PointT GetCenter() const noexcept
        {
            const auto half = T(2);
            return {mX + mWidth/half, mY + mHeight/half};
        }
    private:
        T mX = 0;
        T mY = 0;
        T mWidth = 0;
        T mHeight = 0;
    };

    template<typename T>
    class Circle
    {
    public:
        Circle() = default;
        Circle(float x, float y, float radius) noexcept
          : mX(x)
          , mY(y)
          , mRadius(radius)
        {}
        Circle(const Point<T>& pos, float radius) noexcept
          : mX(pos.GetX())
          , mY(pos.GetY())
          , mRadius(radius)
        {}
        inline T GetRadius() const noexcept
        { return mRadius; }
        inline T GetX() const noexcept
        { return mX; }
        inline T GetY() const noexcept
        { return mY; }
        inline Point<T> GetCenter() const noexcept
        { return {mX, mY}; }
        inline void SetX(T x) noexcept
        { mX = x; }
        inline void SetY(T y) noexcept
        { mY = y; }
        inline void SetRadius(T radius) noexcept
        { mRadius = radius; }
        bool IsEmpty() const noexcept
        {
            return mRadius != T();
        }
        bool TestPoint(const Point<T>& p) const noexcept
        {
            const auto radius_squared = mRadius*mRadius;
            const auto dist_x = p.GetX() - mX;
            const auto dist_y = p.GetY() - mY;
            const auto dist_squared = dist_x*dist_x + dist_y*dist_y;
            return dist_squared < radius_squared;
        }
        void Translate(const Point<T>& p) noexcept
        {
            mX += p.GetX();
            mY += p.GetY();
        }
        void Translate(T x, T y) noexcept
        {
            mX += x;
            mY += y;
        }
        void Move(T x, T y) noexcept
        {
            mX = x;
            mY = y;
        }
        void Move(const Point<T>& pos) noexcept
        {
            mX = pos.GetX();
            mY = pos.GetY();
        }
        Rect<T> Inscribe() const noexcept
        {
            const auto size  = mRadius + mRadius;
            return {mX-mRadius, mY-mRadius, size, size};
        }
    private:
        T mX = 0;
        T mY = 0;
        T mRadius = 0;
    };

    using URect = Rect<unsigned>;
    using FRect = Rect<float>;
    using IRect = Rect<int>;
    using FCircle = Circle<float>;

    inline bool operator==(const URect& lhs, const URect& rhs) noexcept
    {
        return lhs.GetX() == rhs.GetX() &&
               lhs.GetY() == rhs.GetY() &&
               lhs.GetWidth() == rhs.GetWidth() &&
               lhs.GetHeight() == rhs.GetHeight();
    }
    inline bool operator!=(const URect& lhs, const URect& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    inline bool operator==(const IRect& lhs, const IRect& rhs) noexcept
    {
        return lhs.GetX() == rhs.GetX() &&
               lhs.GetY() == rhs.GetY() &&
               lhs.GetWidth() == rhs.GetWidth() &&
               lhs.GetHeight() == rhs.GetHeight();
    }
    inline bool operator!=(const IRect& lhs, const IRect& rhs) noexcept
    {
        return !(lhs == rhs);
    }


    // Test whether the rectangle A contains rectangle B completely.
    template<typename T>
    bool Contains(const Rect<T>& a, const Rect<T>& b) noexcept
    {
        // if all the corners of B are within A then B
        // is completely within A
        const auto [p0, p1, p2, p3] = b.GetCorners();
        if (!a.TestPoint(p0) || !a.TestPoint(p1) ||
            !a.TestPoint(p2) || !a.TestPoint(p3))
            return false;
        return true;
    }

    // Find the intersection of the two rectangles
    // and return the rectangle that represents the intersection.
    template<typename T>
    Rect<T> Intersect(const Rect<T>& lhs, const Rect<T>& rhs) noexcept
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
        if (lhs_right < rhs_left  ||
            lhs_left > rhs_right ||
            lhs_top > rhs_bottom ||
            lhs_bottom < rhs_top)
            return R();

        const auto right  = std::min(lhs_right, rhs_right);
        const auto left   = std::max(lhs_left, rhs_left);
        const auto top    = std::max(lhs_top, rhs_top);
        const auto bottom = std::min(lhs_bottom, rhs_bottom);
        return R(left, top, right - left, bottom - top);
    }

    template<typename T>
    bool DoesIntersect(const Rect<T>& lhs, const Rect<T>& rhs) noexcept
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
        if ((lhs_right < rhs_left) ||
            (lhs_left > rhs_right) ||
            (lhs_top > rhs_bottom) ||
            (lhs_bottom < rhs_top))
            return false;
        return true;
    }

    template<typename T>
    bool DoesIntersect(const Rect<T>& rect, const Circle<T>& circle) noexcept
    {
        return math::CheckRectCircleIntersection(
                rect.GetX(), rect.GetX() + rect.GetWidth(),
                rect.GetY(), rect.GetY() + rect.GetHeight(),
                circle.GetX(), circle.GetY(), circle.GetRadius());
    }
    template<typename T>
    bool DoesIntersect(const Circle<T>& circle, const Rect<T>& rect) noexcept
    { return DoesIntersect(rect, circle); }

    template<typename T>
    Rect<T> Union(const Rect<T>& lhs, const Rect<T>& rhs) noexcept
    {
        using R = Rect<T>;
        if (lhs.IsEmpty())
            return rhs;
        else if (rhs.IsEmpty())
            return lhs;

        const auto lhs_top    = lhs.GetY();
        const auto lhs_left   = lhs.GetX();
        const auto lhs_right  = lhs_left + lhs.GetWidth();
        const auto lhs_bottom = lhs_top + lhs.GetHeight();
        const auto rhs_top    = rhs.GetY();
        const auto rhs_left   = rhs.GetX();
        const auto rhs_right  = rhs_left + rhs.GetWidth();
        const auto rhs_bottom = rhs_top + rhs.GetHeight();

        const auto left   = std::min(lhs_left, rhs_left);
        const auto right  = std::max(lhs_right, rhs_right);
        const auto top    = std::min(lhs_top, rhs_top);
        const auto bottom = std::max(lhs_bottom, rhs_bottom);
        return R(left, top, right - left, bottom - top);
    }

} // namespace
