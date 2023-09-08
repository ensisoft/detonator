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

#include "config.h"

#include "warnpush.h"
#  include <sol/sol.hpp>
#include "warnpop.h"

#include "engine/lua.h"
#include "base/logging.h"
#include "base/trace.h"
#include "game/types.h"

using namespace game;
using namespace engine::lua;

namespace engine
{

void BindBase(sol::state& L)
{
    auto base = L.create_named_table("base");
    base["debug"] = [](const std::string& str) { DEBUG(str); };
    base["warn"]  = [](const std::string& str) { WARN(str); };
    base["error"] = [](const std::string& str) { ERROR(str); };
    base["info"]  = [](const std::string& str) { INFO(str); };

    //auto math  = L.create_named_table("math");
    auto& math = base;
    math["wrap"] = sol::overload(
        [](float min, float max, float value) {
            return math::wrap(min, max, value);
        },
        [](int min, int max, int value) {
            return math::wrap(min, max, value);
        });
    math["clamp"] = sol::overload(
        [](float min, float max, float value) {
            return math::clamp(min, max, value);
        },
        [](int min, int max, int value) {
            return math::clamp(min, max, value);
        });

    auto trace = L.create_named_table("trace");
    trace["marker"] = sol::overload(
            [](const std::string& str) { base::TraceMarker(str); },
            [](const std::string& str, unsigned index) { base::TraceMarker(str, index); }
    );
    trace["enter"]  = &base::TraceBeginScope;
    trace["leave"]  = &base::TraceEndScope;

    sol::constructors<base::FRect(), base::FRect(float, float, float, float)> rect_ctors;
    auto rect = base.new_usertype<base::FRect>("FRect", rect_ctors);
    rect["GetHeight"]      = &base::FRect::GetHeight;
    rect["GetWidth"]       = &base::FRect::GetWidth;
    rect["GetX"]           = &base::FRect::GetX;
    rect["GetY"]           = &base::FRect::GetY;
    rect["SetX"]           = &base::FRect::SetX;
    rect["SetY"]           = &base::FRect::SetY;
    rect["SetWidth"]       = &base::FRect::SetWidth;
    rect["SetHeight"]      = &base::FRect::SetHeight;
    rect["IsEmpty"]        = &base::FRect::IsEmpty;
    rect["Copy"]           = [](const base::FRect& src) { return base::FRect(src); };
    rect["Resize"]         = sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Resize(x, y);
        },
        [](base::FRect& rect, const FSize& point) {
            rect.Resize(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Resize(point.x, point.y);
        });
    rect["Grow"] =  sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Grow(x, y);
        },
        [](base::FRect& rect, const FSize& point) {
            rect.Grow(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Grow(point.x, point.y);
        });
    rect["Move"] =  sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Move(x, y);
        },
        [](base::FRect& rect, const FPoint& point) {
            rect.Move(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Move(point.x, point.y);
        });

    rect["Translate"]  =  sol::overload(
        [](base::FRect& rect, float x, float y) {
            rect.Translate(x, y);
        },
        [](base::FRect& rect, const FPoint& point) {
            rect.Translate(point);
        },
        [](base::FRect& rect, const glm::vec2& point) {
            rect.Translate(point.x, point.y);
        });

    rect["TestPoint"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.TestPoint(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.TestPoint(point);
        },
        [](const base::FRect& rect, const glm::vec2& point) {
            return rect.TestPoint(point.x, point.y);
        });

    rect["MapToGlobal"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.MapToGlobal(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.MapToGlobal(point);
        },
        [](const base::FRect& rect, const glm::vec2& point) {
           return rect.MapToGlobal(point.x, point.y);
        });
    rect["MapToLocal"] = sol::overload(
        [](const base::FRect& rect, float x, float y) {
            return rect.MapToLocal(x, y);
        },
        [](const base::FRect& rect, const FPoint& point) {
            return rect.MapToLocal(point);
        },
        [](const base::FRect& rect, const glm::vec2& point) {
            return rect.MapToLocal(point.x, point.y);
        });
    rect["GetQuadrants"]   = &base::FRect::GetQuadrants;
    rect["GetCorners"]     = &base::FRect::GetCorners;
    rect["GetCenter"]      = &base::FRect::GetCenter;
    rect["Combine"]        = &base::Union<float>;
    rect["Intersect"]      = &base::Intersect<float>;
    rect["TestIntersect"]  = [](const base::FRect& lhs, const base::FRect& rhs) { return base::DoesIntersect(lhs, rhs); };
    rect.set_function(sol::meta_function::to_string, [](const FRect& rect) {
        return base::ToString(rect);
    });

    sol::constructors<base::FSize(), base::FSize(float, float)> size_ctors;
    auto size = base.new_usertype<base::FSize>("FSize", size_ctors);
    size["GetWidth"]  = &base::FSize::GetWidth;
    size["GetHeight"] = &base::FSize::GetHeight;
    size["IsZero"]    = &base::FSize::IsZero;
    size["Copy"]      = [](const base::FSize& size) { return base::FSize(size); };
    size.set_function(sol::meta_function::multiplication, [](const base::FSize& size, float scalar) { return size * scalar; });
    size.set_function(sol::meta_function::addition, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs + rhs; });
    size.set_function(sol::meta_function::subtraction, [](const base::FSize& lhs, const base::FSize& rhs) { return lhs - rhs; });
    size.set_function(sol::meta_function::to_string, [](const base::FSize& size) { return base::ToString(size); });

    sol::constructors<base::FPoint(), base::FPoint(float, float)> point_ctors;
    auto point = base.new_usertype<base::FPoint>("FPoint", point_ctors);
    point["GetX"] = &base::FPoint::GetX;
    point["GetY"] = &base::FPoint::GetY;
    point["SetX"] = &base::FPoint::SetX;
    point["SetY"] = &base::FPoint::SetY;
    point["Copy"] = [](const base::FPoint& point) { return base::FPoint(point); };
    point["Distance"] = &base::Distance;
    point["SquareDistance"] = &base::Distance;

    point.set_function(sol::meta_function::addition, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs + rhs; });
    point.set_function(sol::meta_function::subtraction, [](const base::FPoint& lhs, const base::FPoint& rhs) { return lhs - rhs; });
    point.set_function(sol::meta_function::to_string, [](const base::FPoint& point) { return base::ToString(point); });

    // build color name table
    for (const auto& color : magic_enum::enum_values<base::Color>())
    {
        const std::string name(magic_enum::enum_name(color));
        base[sol::create_if_nil]["Colors"][name] = magic_enum::enum_integer(color);
    }

    // todo: figure out a way to construct from color name, is that possible?
    sol::constructors<base::Color4f(),
            base::Color4f(float, float, float, float),
            base::Color4f(int, int, int, int)> color_ctors;
    auto color = base.new_usertype<base::Color4f>("Color4f", color_ctors);
    color["GetRed"]     = &base::Color4f::Red;
    color["GetGreen"]   = &base::Color4f::Green;
    color["GetBlue"]    = &base::Color4f::Blue;
    color["GetAlpha"]   = &base::Color4f::Alpha;
    color["Copy"]       = [](const base::Color4f& color) { return base::Color4f(color); };
    color["SetRed"]     = (void(base::Color4f::*)(float))&base::Color4f::SetRed;
    color["SetGreen"]   = (void(base::Color4f::*)(float))&base::Color4f::SetGreen;
    color["SetBlue"]    = (void(base::Color4f::*)(float))&base::Color4f::SetBlue;
    color["SetAlpha"]   = (void(base::Color4f::*)(float))&base::Color4f::SetAlpha;
    color["SetColor"]   = sol::overload(
        [](base::Color4f& color, int value) {
            const auto color_value = magic_enum::enum_cast<base::Color>(value);
            if (!color_value.has_value())
                throw GameError("No such color value:" + std::to_string(value));
            color = base::Color4f(color_value.value());
        },
        [](base::Color4f& color, const std::string& name) {
            const auto color_value = magic_enum::enum_cast<base::Color>(name);
            if (!color_value.has_value())
                throw GameError("No such color name: " + name);
            color = base::Color4f(color_value.value());
        });
    color["FromEnum"] = sol::overload(
        [](int value) {
            const auto color_value = magic_enum::enum_cast<base::Color>(value);
            if (!color_value.has_value())
                throw GameError("No such color value:" + std::to_string(value));
            return base::Color4f(color_value.value());
        },
        [](const std::string& name) {
            const auto color_value = magic_enum::enum_cast<base::Color>(name);
            if (!color_value.has_value())
                throw GameError("No such color name: " + name);
            return base::Color4f(color_value.value());
        });
    color.set_function(sol::meta_function::to_string, [](const base::Color4f& color) { return base::ToString(color); });
}

} // namespace