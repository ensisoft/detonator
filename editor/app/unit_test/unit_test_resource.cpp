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

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/utility.h"
#include "base/json.h"
#include "editor/app/resource.h"

class TestResource0
{
public:
    TestResource0()
    { mClassId = base::RandomString(10); }
    std::string GetId() const
    { return mClassId; }

    void SetFloat1(float val)
    { mFloatProp1 = val; }
    void SetFloat2(float val)
    { mFloatProp2 = val; }
    void SetString(std::string str)
    { mStringProp = std::move(str); }

    TestResource0 Clone() const
    {
        TestResource0 ret;
        ret.mClassId = base::RandomString(10);
        ret.mFloatProp1 = mFloatProp1;
        ret.mFloatProp2 = mFloatProp2;
        ret.mStringProp = mStringProp;
        return ret;
    }
    void IntoJson(data::Writer& data) const
    {
        data.Write("id", mClassId);
        data.Write("float1", mFloatProp1);
        data.Write("float2", mFloatProp2);
        data.Write("string", mStringProp);
    }
    void SetName(const std::string& name)
    {}
private:
    std::string mClassId;
    float mFloatProp1 = 0.0f;
    float mFloatProp2 = 0.0;
    std::string mStringProp;
};

class BaseResource
{
public:
    virtual ~BaseResource() = default;
};

class TestResource1 : public BaseResource
{
public:
    TestResource1()
    { mClassId = base::RandomString(10); }

    std::string GetId() const
    { return mClassId; }
    std::unique_ptr<BaseResource> Clone() const
    {
        return std::make_unique<TestResource1>(*this);
    }
    void IntoJson(data::Writer&) const
    {
    }
private:
    std::string mClassId;
};

namespace app {
    namespace detail {
        template<>
        struct ResourceTypeTraits<TestResource0> {
            static constexpr auto Type = app::Resource::Type::Material;
        };
        template<>
        struct ResourceTypeTraits<TestResource1> {
            static constexpr auto Type = app::Resource::Type::ParticleSystem;
        };
        template<>
        struct ResourceTypeTraits<BaseResource> {
            static constexpr auto Type = app::Resource::Type::Drawable;
        };
    }
}

int test_main(int argc, char* argv[])
{
    TestResource0 res1;
    res1.SetFloat1(123.0f);
    res1.SetFloat2(321.0f);
    res1.SetString("foo");

    TestResource0 res2;
    res2.SetFloat1(123.0f);
    res2.SetFloat2(321.0f);
    res2.SetString("bar");

    TestResource1 res3;

    using Resource = app::GameResource<TestResource0>;
    Resource r(res1, "test");
    TEST_REQUIRE(r.GetId() == app::FromUtf8(res1.GetId()));
    TEST_REQUIRE(r.GetIdUtf8() == res1.GetId());
    TEST_REQUIRE(r.GetName() == "test");
    TEST_REQUIRE(r.GetNameUtf8() == "test");
    TEST_REQUIRE(r.GetType() == app::Resource::Type::Material);
    TEST_REQUIRE(r.IsPrimitive() == false);
    r.SetName("joojoo");
    r.SetIsPrimitive(true);
    TEST_REQUIRE(r.GetName() == "joojoo");
    TEST_REQUIRE(r.GetNameUtf8() == "joojoo");
    TEST_REQUIRE(r.IsPrimitive() == true);

    QByteArray bytes;
    bytes.append("byte array string");

    QVariantMap map;
    map["value"] = 123;
    map["string"] = "boo";
    r.SetProperty("variant_map", map);
    r.SetProperty("int", 123);
    r.SetProperty("float", 123.0f);
    r.SetProperty("string", QString("hip hop"));
    r.SetProperty("double", 123.0);
    r.SetProperty("ulonglong", quint64(123));
    r.SetProperty("longlong", qint64(7879));
    r.SetProperty("bytes", bytes);
    r.SetProperty("color", QColor::fromRgb(100, 120, 120, 200));
    r.SetProperty("utf8-string", std::string("bla bla"));
    QJsonObject json;
    app::JsonWrite(json, "foo", QString("foobar"));
    app::JsonWrite(json, "int", 123);
    r.SetProperty("json", json);

    r.SetUserProperty("int", 42);
    r.SetUserProperty("bytes", bytes);
    r.SetUserProperty("color", QColor::fromRgb(50, 80, 90, 120));
    r.SetUserProperty("utf8-string-user", std::string("joojoo"));
    r.SetUserProperty("json-user", json);
    map.clear();


    TEST_REQUIRE(r.HasProperty("int"));
    TEST_REQUIRE(r.HasProperty("float"));
    TEST_REQUIRE(r.HasProperty("string"));
    TEST_REQUIRE(r.HasProperty("double"));
    TEST_REQUIRE(r.HasProperty("ulonglong"));
    TEST_REQUIRE(r.HasProperty("longlong"));
    TEST_REQUIRE(r.HasProperty("variant_map"));
    TEST_REQUIRE(r.HasProperty("bytes"));
    TEST_REQUIRE(r.HasProperty("baz") == false);

    TEST_REQUIRE(r.HasUserProperty("bar") == false);
    TEST_REQUIRE(r.HasUserProperty("bytes"));

    TEST_REQUIRE(r.GetProperty("int", 0) == 123);
    TEST_REQUIRE(r.GetProperty("string", QString("")) == "hip hop");
    TEST_REQUIRE(r.GetProperty("ulonglong", quint64(0)) == quint64(123));
    TEST_REQUIRE(r.GetProperty("longlong", qint64(0)) == qint64(7879));
    TEST_REQUIRE(r.GetProperty("double", 0.0) == real::float32(123.0));
    TEST_REQUIRE(r.GetProperty("float", 0.0f) == real::float32(123.0f));
    TEST_REQUIRE(r.GetProperty("bytes", QByteArray()) == bytes);
    TEST_REQUIRE(r.GetProperty("color", QColor()) == QColor::fromRgb(100, 120, 120, 200));
    TEST_REQUIRE(r.GetProperty("utf8-string", std::string()) == "bla bla");
    map = r.GetProperty("variant_map", map);
    TEST_REQUIRE(map["value"].toInt() == 123);
    TEST_REQUIRE(map["string"].toString() == "boo");
    QJsonObject json_out;
    TEST_REQUIRE(r.GetProperty("json", &json_out));
    TEST_REQUIRE(json_out["foo"].toString() == "foobar");
    TEST_REQUIRE(json_out["int"].toInt() == 123);

    TEST_REQUIRE(r.GetUserProperty("int", 0) == 42);
    TEST_REQUIRE(r.GetUserProperty("bytes", QByteArray()) == bytes);
    TEST_REQUIRE(r.GetUserProperty("color", QColor()) == QColor::fromRgb(50, 80, 90, 120));
    TEST_REQUIRE(r.GetUserProperty("utf8-string-user", std::string()) == "joojoo");
    QJsonObject json_out_user;
    TEST_REQUIRE(r.GetUserProperty("json-user", &json_out_user));
    TEST_REQUIRE(json_out_user["foo"].toString() == "foobar");
    TEST_REQUIRE(json_out_user["int"].toInt() == 123);

    QJsonObject props;
    QJsonObject user_props;
    r.SaveProperties(props);
    r.SaveUserProperties(user_props);
    r.ClearProperties();
    r.ClearUserProperties();
    TEST_REQUIRE(!r.HasProperty("int"));
    TEST_REQUIRE(!r.HasProperty("string"));
    TEST_REQUIRE(!r.HasProperty("float"));
    TEST_REQUIRE(!r.HasProperty("double"));
    TEST_REQUIRE(!r.HasProperty("longlong"));
    r.LoadProperties(props);
    r.LoadUserProperties(user_props);
    TEST_REQUIRE(r.HasProperty("variant_map"));
    TEST_REQUIRE(r.HasProperty("int"));
    TEST_REQUIRE(r.HasProperty("string"));
    TEST_REQUIRE(r.HasProperty("float"));
    TEST_REQUIRE(r.HasProperty("double"));
    TEST_REQUIRE(r.HasProperty("longlong"));
    TEST_REQUIRE(r.GetProperty("utf8-string", std::string()) == "bla bla");

    TEST_REQUIRE(r.HasUserProperty("int"));
    TEST_REQUIRE(r.HasUserProperty("bytes"));
    TEST_REQUIRE(r.GetUserProperty("utf8-string-user", std::string()) == "joojoo");

    TestResource0* ptr0 = nullptr;
    TestResource1* ptr1 = nullptr;
    TEST_REQUIRE(r.GetContent(&ptr0));
    TEST_REQUIRE(r.GetContent(&ptr1) == false);
    TEST_REQUIRE(ptr0->GetId() == res1.GetId());

    // copy
    {
        Resource copy(r);
        TEST_REQUIRE(copy.GetIdUtf8() == res1.GetId());
        TEST_REQUIRE(copy.GetName() == "joojoo");
        TEST_REQUIRE(copy.HasProperty("int"));
        TEST_REQUIRE(copy.HasProperty("string"));
        TEST_REQUIRE(copy.HasProperty("float"));
        TEST_REQUIRE(copy.HasProperty("double"));
        TEST_REQUIRE(copy.HasProperty("longlong"));
        TEST_REQUIRE(copy.HasUserProperty("int"));
    }

    // copy
    {
        auto copy = r.Copy();
        TEST_REQUIRE(copy->GetIdUtf8() == res1.GetId());
        TEST_REQUIRE(copy->GetName() == "joojoo");
        TEST_REQUIRE(copy->HasProperty("int"));
        TEST_REQUIRE(copy->HasProperty("string"));
        TEST_REQUIRE(copy->HasProperty("float"));
        TEST_REQUIRE(copy->HasProperty("longlong"));
        TEST_REQUIRE(copy->HasProperty("ulonglong"));
        TEST_REQUIRE(copy->HasUserProperty("int"));
    }

    // clone
    {
        auto clone = r.Clone();
        TEST_REQUIRE(clone->GetIdUtf8() != res1.GetId());
        TEST_REQUIRE(clone->GetName() == "joojoo");
        TEST_REQUIRE(clone->HasProperty("int"));
        TEST_REQUIRE(clone->HasProperty("string"));
        TEST_REQUIRE(clone->HasProperty("float"));
        TEST_REQUIRE(clone->HasProperty("longlong"));
        TEST_REQUIRE(clone->HasProperty("ulonglong"));
        TEST_REQUIRE(clone->HasUserProperty("int"));
    }

    // update
    {
        Resource other("other");
        // copies over the data from r into other
        other.UpdateFrom(r);
        TEST_REQUIRE(other.GetName() == "joojoo");
        TEST_REQUIRE(other.HasProperty("int"));
        TEST_REQUIRE(other.HasProperty("string"));
        TEST_REQUIRE(other.HasProperty("float"));
        TEST_REQUIRE(other.HasProperty("longlong"));
        TEST_REQUIRE(other.HasProperty("bytes"));
        TEST_REQUIRE(other.HasUserProperty("int"));
    }

    // delete.
    {
        r.DeleteProperty("int");
        r.DeleteUserProperty("int");
        TEST_REQUIRE(r.HasProperty("int") == false);
        TEST_REQUIRE(r.HasUserProperty("int") == false);
    }

    // clone with a type that returns std::unique_ptr
    {
        using Resource = app::GameResource<BaseResource, TestResource1>;
        TestResource1 test;
        Resource r(test, "test");
        auto clone = r.Clone();
    }

    return 0;
}
