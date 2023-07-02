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
#  include <QEvent>
#  include <QPoint>
#  include <QPointF>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <atomic>
#include <tuple>

#include "base/types.h"

namespace gui
{
    class GameLoopEvent : public QEvent
    {
    public:
        GameLoopEvent() : QEvent(GetIdentity())
        {
            ++GetCounter();
        }
       ~GameLoopEvent()
        {
           --GetCounter();
        }
        GameLoopEvent(const GameLoopEvent&) = delete;

        static QEvent::Type GetIdentity()
        {
            static auto id = QEvent::registerEventType();
            return (QEvent::Type)id;
        }
        GameLoopEvent& operator=(const GameLoopEvent&) = delete;

        static std::atomic<size_t>& GetCounter()
        {
            static std::atomic<size_t> counter;
            return counter;
        }
        static bool IsPending()
        {
            return GetCounter() > 0;
        }
    private:
    };


    // silly wrapper class to facilitate automatic conversions from
    // various different "point/coord/vec2" types to a single point
    // type and then back automatically.
    // Intention is not to use this for computation per se but just
    // for carrying data into and out of different functions that
    // then perform some computations.
    class Point2Df
    {
    public:
        using Tuple = std::tuple<float, float>;

        Point2Df(float x, float y) noexcept
          : mX(x)
          , mY(y)
        {}
        Point2Df(const QPointF& point) noexcept
          : mX(point.x())
          , mY(point.y())
        {}
        Point2Df(const QPoint& point) noexcept
          : mX(point.x())
          , mY(point.y())
        {}
        Point2Df(const glm::vec2& point) noexcept
          : mX(point.x)
          , mY(point.y)
        {}
        Point2Df(const base::FPoint& point) noexcept
          : mX(point.GetX())
          , mY(point.GetY())
        {}
        operator QPointF () const noexcept
        { return {mX, mY}; }
        operator glm::vec2 () const noexcept
        { return {mX, mY}; }
        operator base::FPoint () const noexcept
        { return {mX, mY}; }
        operator Tuple () const noexcept
        { return {mX, mY}; }
    private:
        float mX = 0.0f;
        float mY = 0.0f;
    };

    class Size2Df
    {
    public:
        using Tuple = std::tuple<float, float>;

        Size2Df(float width, float height) noexcept
          : mW(width)
          , mH(height)
        {}
        Size2Df(const QSizeF& size) noexcept
          : mW(size.width())
          , mH(size.height())
        {}
        Size2Df(const QSize& size) noexcept
          : mW(size.width())
          , mH(size.height())
        {}
        Size2Df(const glm::vec2& size) noexcept
          : mW(size.x)
          , mH(size.y)
        {}
        Size2Df(const base::FSize& size) noexcept
          : mW(size.GetWidth())
          , mH(size.GetHeight())
        {}
        operator QSizeF () const noexcept
        { return {mW, mH}; }
        operator glm::vec2 () const noexcept
        { return {mW, mH}; }
        operator base::FSize () const noexcept
        { return {mW, mH}; }
        operator Tuple () const noexcept
        { return {mW, mH}; }
    private:
        float mW = 0.0f;
        float mH = 0.0f;
    };

    class Rect2Df
    {
    public:
        Rect2Df(float x, float y, float w, float h) noexcept
          : mX(x)
          , mY(y)
          , mWidth(w)
          , mHeight(h)
        {}
        Rect2Df(const QRect& rect) noexcept
          : mX(rect.x())
          , mY(rect.y())
          , mWidth(rect.width())
          , mHeight(rect.height())
        {}
        Rect2Df(const QRectF& rect) noexcept
          : mX(rect.x())
          , mY(rect.y())
          , mWidth(rect.width())
          , mHeight(rect.height())
        {}
        Rect2Df(const base::FRect& rect) noexcept
          : mX(rect.GetX())
          , mY(rect.GetY())
          , mWidth(rect.GetWidth())
          , mHeight(rect.GetHeight())
        {}
        operator QRectF () const noexcept
        { return {mX, mY, mWidth, mHeight}; }
        operator base::FRect () const noexcept
        { return {mX, mY, mWidth, mHeight}; }

    private:
        float mX = 0.0f;
        float mY = 0.0f;
        float mWidth = 0.0f;
        float mHeight = 0.0f;
    };


} // namespace
