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
#  include <boost/random/mersenne_twister.hpp>
#  include <boost/random/uniform_int_distribution.hpp>
#  include <boost/random/uniform_real_distribution.hpp>
#  include <sol/sol.hpp>
#include "warnpop.h"

#include <chrono>

#include "base/utility.h"
#include "base/format.h"
#include "game/util.h"
#include "game/entity.h"
#include "engine/lua.h"
#include "engine/lua_array.h"
#include "engine/classlib.h"
#include "graphics/material.h"

using namespace game;
using namespace engine::lua;

namespace {
// the problem with using a std random number generation is that
// the results may not be portable across implementations and it seems
// that the standard Lua math random stuff has this problem.
// http://lua-users.org/wiki/MathLibraryTutorial
// "... math.randomseed will call the underlying C function srand ..."
class RandomEngine {
public:
    void Seed(int seed)
    { mTwister = boost::random::mt19937(seed); }
    int NextInt()
    { return NextInt(INT_MIN, INT_MAX); }
    int NextInt(int min, int max)
    {
        boost::random::uniform_int_distribution<int> dist(min, max);
        return dist(mTwister);
    }
    float NextFloat(float min, float max)
    {
        boost::random::uniform_real_distribution<float> dist(min, max);
        return dist(mTwister);
    }
    static RandomEngine& Get()
    {
        static RandomEngine instance;
        return instance;
    }
    static void SeedGlobal(int seed)
    {
        Get().Seed(seed);
    }
    static int NextIntGlobal()
    {
        return Get().NextInt();
    }
    static int NextIntGlobal(int min, int max)
    {
        return Get().NextInt(min, max);
    }
    static float NextFloatGlobal(float min, float max)
    {
        return Get().NextFloat(min, max);
    }
private:
    boost::random::mt19937 mTwister;
};

template<typename T, template<typename> class DataPolicy = ArrayDataPointer>
void BindArrayInterface(sol::table& table, const char* name)
{
    using Type = ArrayInterface<T, DataPolicy>;

    // regarding the array indexing for the subscript operator.
    // Lua uses 1 based indexing and allows (with built-in arrays)
    // access to indices that don't exist. For example, you can do
    //   local foo = {'foo', 'bar'}
    //   print(foo[0])
    //   print(foo[3])
    //
    // this will print nil twice.
    // Lua also allows for holes to be had in the array. for example
    //    foo[4] = 'keke'
    //    print(foo[4])
    //    print(foo[3])
    //
    // will print keke followed by nil
    //
    // Going to stick to more C++ like semantics here and say that
    // trying to access an index that doesn't exist is Lua application error.
    auto arr = table.new_usertype<Type>(name,
        sol::meta_function::index, &Type::GetItemFromLua,
        sol::meta_function::new_index, &Type::SetItemFromLua);
    arr["IsEmpty"]    = &Type::empty;
    arr["Size"]       = &Type::size;
    arr["IsReadOnly"] = &Type::IsReadOnly;
    arr["GetItem"]    = &Type::GetItemFromLua;
    arr["SetItem"]    = &Type::SetItemFromLua;
    arr["PopBack"]    = &Type::PopBack;
    arr["PopFront"]   = &Type::PopFront;
    arr["First"]      = &Type::GetFirst;
    arr["Last"]       = &Type::GetLast;
    arr["PushBack"]   = &Type::push_back;
    arr["Clear"]      = &Type::Clear;
}

} // namespace

namespace engine
{

 void BindUtil(sol::state& L)
{
    auto util = L.create_named_table("util");
    util["GetRotationFromMatrix"]    = &GetRotationFromMatrix;
    util["GetScaleFromMatrix"]       = &GetScaleFromMatrix;
    util["GetTranslationFromMatrix"] = &GetTranslationFromMatrix;
    util["RotateVector"]             = &RotateVector;
    util["ToVec2"] = [](const base::FPoint& point) {
        return glm::vec2(point.GetX(), point.GetY());
    };
    util["ToPoint"] = [](const glm::vec2& vec2) {
        return base::FPoint(vec2.x, vec2.y);
    };

    // see comments at RandomEngine about why this is done.
    util["RandomSeed"] = &RandomEngine::SeedGlobal;
    util["Random"] = sol::overload(
        [](sol::this_state state) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextIntGlobal());
        },
        [](sol::this_state state, int min, int max) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextIntGlobal(min, max));
        },
        [](sol::this_state state, float min, float max) {
            sol::state_view view(state);
            return sol::make_object(view, RandomEngine::NextFloatGlobal(min, max));
        });

    util["GetSeconds"] = []() {
        using clock = std::chrono::high_resolution_clock;
        const auto now  = clock::now();
        const auto gone = now.time_since_epoch();
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(gone);
        return us.count() / (1000.0 * 1000.0);
    };
    util["GetMilliseconds"] = []() {
        using clock = std::chrono::high_resolution_clock;
        const auto now = clock::now();
        const auto gone = now.time_since_epoch();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(gone);
        return ms.count();
    };

    sol::constructors<RandomEngine()> random_engine_ctor;
    auto random_engine = util.new_usertype<RandomEngine>("RandomEngine", random_engine_ctor);
    random_engine["Seed"] = &RandomEngine::Seed;
    random_engine["Random"] = sol::overload(
        [](RandomEngine& engine, sol::this_state state) {
            sol::state_view view(state);
            return sol::make_object(view, engine.NextInt());
        },
        [](RandomEngine& engine, sol::this_state state, int min, int max) {
            sol::state_view view(state);
            return sol::make_object(view, engine.NextInt(min, max));
        },
        [](RandomEngine& engine, sol::this_state state, float min, float max) {
            sol::state_view view(state);
            return sol::make_object(view, engine.NextFloat(min, max));
        });

    sol::constructors<FBox(),
            FBox(float, float),
            FBox(const glm::mat4& mat, float, float),
            FBox(const glm::mat4& mat)> ctors;
    auto box = util.new_usertype<FBox>("FBox", ctors);
    box["GetWidth"]    = &FBox::GetWidth;
    box["GetHeight"]   = &FBox::GetHeight;
    box["GetTopLeft"]  = &FBox::GetTopLeft;
    box["GetTopRight"] = &FBox::GetTopRight;
    box["GetBotRight"] = &FBox::GetTopRight;
    box["GetBotLeft"]  = &FBox::GetBotLeft;
    box["GetCenter"]   = &FBox::GetCenter;
    box["GetSize"]     = &FBox::GetSize;
    box["GetRotation"] = &FBox::GetRotation;
    box["Transform"]   = &FBox::Transform;
    box["Reset"]       = sol::overload(
            [](FBox& box) {box.Reset(); },
            [](FBox& box, float width, float height) { box.Reset(width, height); }
            );

    util["JoinPath"]   = &base::JoinPath;
    util["FileExists"] = &base::FileExists;
    util["RandomString"] = &base::RandomString;

    util["FormatString"] = [](std::string str, sol::variadic_args args) {
        for (size_t i=0; i<args.size(); ++i) {
            const auto& arg = args[i];
            const auto index = i + 1;
            if (arg.is<std::string>())
                str = base::detail::ReplaceIndex(index, str, arg.get<std::string>());
            else if (arg.is<int>())
                str = base::detail::ReplaceIndex(index, str, arg.get<int>());
            else if (arg.is<float>())
                str = base::detail::ReplaceIndex(index, str, arg.get<float>());
            else if (arg.is<bool>())
                str = base::detail::ReplaceIndex(index, str, arg.get<bool>());
            else if (arg.is<base::FSize>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::FSize>());
            else if (arg.is<base::FPoint>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::FPoint>());
            else if (arg.is<base::FRect>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::FRect>());
            else if (arg.is<base::Color4f>())
                str = base::detail::ReplaceIndex(index, str, arg.get<base::Color4f>());
            else if (arg.is<glm::vec2>())
                str = base::detail::ReplaceIndex(index, str, arg.get<glm::vec2>());
            else if (arg.is<glm::vec3>())
                str = base::detail::ReplaceIndex(index, str, arg.get<glm::vec3>());
            else if (arg.is<glm::vec4>())
                str = base::detail::ReplaceIndex(index, str, arg.get<glm::vec4>());
            else throw GameError("Unsupported string format value type.");
        }
        return str;
    };

    using MaterialClassHandle = engine::ClassLibrary::ClassHandle<const gfx::MaterialClass>;

    BindArrayInterface<int,         ArrayDataPointer>(util, "IntArrayInterface");
    BindArrayInterface<float,       ArrayDataPointer>(util, "FloatArrayInterface");
    BindArrayInterface<bool,        ArrayDataPointer>(util, "BoolArrayInterface");
    BindArrayInterface<std::string, ArrayDataPointer>(util, "StringArrayInterface");
    BindArrayInterface<glm::vec2,   ArrayDataPointer>(util, "Vec2ArrayInterface");
    BindArrayInterface<std::string, ArrayDataObject>(util, "StringArray");
    BindArrayInterface<Entity*,     ArrayObjectReference>(util, "EntityRefArray");
    BindArrayInterface<EntityNode*, ArrayObjectReference>(util, "EntityNodeRefArray");
    BindArrayInterface<MaterialClassHandle, ArrayDataObject>(util, "MaterialRefArray");

    util["Join"] = [](const ArrayInterface<std::string>& array, const std::string& separator) {
        std::string ret;
        for (unsigned i=0; i<array.size(); ++i) {
            ret += array.GetItem(i);
            ret += separator;
        }
        return ret;
    };
}

} // namespace