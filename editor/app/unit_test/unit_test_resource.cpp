
// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
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

#include "config.h"

#include <string>

#include <nlohmann/json.hpp>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/utility.h"
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
    nlohmann::json ToJson() const
    {
        nlohmann::json json;
        base::JsonWrite(json, "id", mClassId);
        base::JsonWrite(json, "float1", mFloatProp1);
        base::JsonWrite(json, "float2", mFloatProp2);
        base::JsonWrite(json, "string", mStringProp);
        return json;
    }
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
    nlohmann::json ToJson() const
    {
        nlohmann::json json;
        return json;
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

    QVariantMap map;
    map["value"] = 123;
    map["string"] = "boo";
    r.SetProperty("variant_map", map);
    r.SetProperty("eka", 123);
    r.SetProperty("toka", 123.0f);
    r.SetProperty("koli", QString("hip hop"));
    r.SetProperty("neli", 123.0);
    r.SetProperty("vika", quint64(123));
    r.SetUserProperty("foo", 42);
    map.clear();

    TEST_REQUIRE(r.HasProperty("eka"));
    TEST_REQUIRE(r.HasProperty("toka"));
    TEST_REQUIRE(r.HasProperty("koli"));
    TEST_REQUIRE(r.HasProperty("neli"));
    TEST_REQUIRE(r.HasProperty("vika"));
    TEST_REQUIRE(r.HasProperty("variant_map"));
    TEST_REQUIRE(r.HasProperty("baz") == false);
    TEST_REQUIRE(r.HasUserProperty("bar") == false);

    TEST_REQUIRE(r.GetProperty("eka", 0) == 123);
    TEST_REQUIRE(r.GetProperty("koli", QString("")) == "hip hop");
    TEST_REQUIRE(r.GetProperty("vika", quint64(0)) == quint64(123));
    TEST_REQUIRE(r.GetProperty("neli", 0.0) == real::float32(123.0));
    TEST_REQUIRE(r.GetProperty("toka", 0.0f) == real::float32(123.0f));
    TEST_REQUIRE(r.GetUserProperty("foo", 0) == 42);
    map = r.GetProperty("variant_map", map);
    TEST_REQUIRE(map["value"].toInt() == 123);
    TEST_REQUIRE(map["string"].toString() == "boo");

    QJsonObject props;
    QJsonObject user_props;
    r.SaveProperties(props);
    r.SaveUserProperties(user_props);
    r.ClearProperties();
    r.ClearUserProperties();
    TEST_REQUIRE(!r.HasProperty("eka"));
    TEST_REQUIRE(!r.HasProperty("toka"));
    TEST_REQUIRE(!r.HasProperty("koli"));
    TEST_REQUIRE(!r.HasProperty("neli"));
    TEST_REQUIRE(!r.HasProperty("vika"));
    r.LoadProperties(props);
    r.LoadUserProperties(user_props);
    TEST_REQUIRE(r.HasProperty("variant_map"));
    TEST_REQUIRE(r.HasProperty("eka"));
    TEST_REQUIRE(r.HasProperty("toka"));
    TEST_REQUIRE(r.HasProperty("koli"));
    TEST_REQUIRE(r.HasProperty("neli"));
    TEST_REQUIRE(r.HasProperty("vika"));
    TEST_REQUIRE(r.HasUserProperty("foo"));

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
        TEST_REQUIRE(copy.HasProperty("eka"));
        TEST_REQUIRE(copy.HasProperty("toka"));
        TEST_REQUIRE(copy.HasProperty("koli"));
        TEST_REQUIRE(copy.HasProperty("neli"));
        TEST_REQUIRE(copy.HasProperty("vika"));
        TEST_REQUIRE(copy.HasUserProperty("foo"));
    }

    // copy
    {
        auto copy = r.Copy();
        TEST_REQUIRE(copy->GetIdUtf8() == res1.GetId());
        TEST_REQUIRE(copy->GetName() == "joojoo");
        TEST_REQUIRE(copy->HasProperty("eka"));
        TEST_REQUIRE(copy->HasProperty("toka"));
        TEST_REQUIRE(copy->HasProperty("koli"));
        TEST_REQUIRE(copy->HasProperty("neli"));
        TEST_REQUIRE(copy->HasProperty("vika"));
        TEST_REQUIRE(copy->HasUserProperty("foo"));
    }

    // clone
    {
        auto clone = r.Clone();
        TEST_REQUIRE(clone->GetIdUtf8() != res1.GetId());
        TEST_REQUIRE(clone->GetName() == "joojoo");
        TEST_REQUIRE(clone->HasProperty("eka"));
        TEST_REQUIRE(clone->HasProperty("toka"));
        TEST_REQUIRE(clone->HasProperty("koli"));
        TEST_REQUIRE(clone->HasProperty("neli"));
        TEST_REQUIRE(clone->HasProperty("vika"));
        TEST_REQUIRE(clone->HasUserProperty("foo"));
    }

    // update
    {
        Resource other("other");
        // copies over the data from r into other
        other.UpdateFrom(r);
        TEST_REQUIRE(other.GetName() == "joojoo");
        TEST_REQUIRE(other.HasProperty("eka"));
        TEST_REQUIRE(other.HasProperty("toka"));
        TEST_REQUIRE(other.HasProperty("koli"));
        TEST_REQUIRE(other.HasProperty("neli"));
        TEST_REQUIRE(other.HasProperty("vika"));
        TEST_REQUIRE(other.HasUserProperty("foo"));
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
