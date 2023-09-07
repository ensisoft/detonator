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
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <tuple>
#include <string>

#include "base/types.h"
#include "base/color4f.h"
#include "base/utility.h"
#include "engine/lua.h"
#include "data/io.h"
#include "data/json.h"
#include "data/reader.h"
#include "data/writer.h"

// sol overload resolution requires a define
// SOL_ALL_SAFETIES_ON will take care of that

#ifndef SOL_ALL_SAFETIES_ON
#  error we need SOL safety flags for correct function
#else
#  pragma message "SOL SAFETIES ARE ON  !"
#endif

using namespace data;
using namespace engine::lua;

namespace {
template<typename Writer>
void BindDataWriterInterface(sol::usertype<Writer>& writer)
{
    writer["Write"] = sol::overload(
        (void(Writer::*)(const char*, int))&Writer::Write,
        (void(Writer::*)(const char*, float))&Writer::Write,
        (void(Writer::*)(const char*, bool))&Writer::Write,
        (void(Writer::*)(const char*, const std::string&))&Writer::Write,
        (void(Writer::*)(const char*, const glm::vec2&))&Writer::Write,
        (void(Writer::*)(const char*, const glm::vec3&))&Writer::Write,
        (void(Writer::*)(const char*, const glm::vec4&))&Writer::Write,
        (void(Writer::*)(const char*, const base::FRect&))&Writer::Write,
        (void(Writer::*)(const char*, const base::FPoint&))&Writer::Write,
        (void(Writer::*)(const char*, const base::FSize&))&Writer::Write,
        (void(Writer::*)(const char*, const base::Color4f&))&Writer::Write);
    writer["HasValue"]      = &Writer::HasValue;
    writer["NewWriteChunk"] = &Writer::NewWriteChunk;
    writer["AppendChunk"]   = (void(Writer::*)(const char*, const data::Writer&))&Writer::AppendChunk;
}

template<typename Reader>
void BindDataReaderInterface(sol::usertype<Reader>& reader)
{
    reader["ReadFloat"]   = (std::tuple<bool, float>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadInt"]     = (std::tuple<bool, int>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadBool"]    = (std::tuple<bool, bool>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadString"]  = (std::tuple<bool, std::string>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadVec2"]    = (std::tuple<bool, glm::vec2>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadVec3"]    = (std::tuple<bool, glm::vec3>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadVec4"]    = (std::tuple<bool, glm::vec4>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadFRect"]   = (std::tuple<bool, base::FRect>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadFPoint"]  = (std::tuple<bool, base::FPoint>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadFSize"]   = (std::tuple<bool, base::FSize>(Reader::*)(const char*)const)&Reader::Read;
    reader["ReadColor4f"] = (std::tuple<bool, base::Color4f>(Reader::*)(const char*)const)&Reader::Read;

    reader["Read"] = sol::overload(
      (std::tuple<bool, float>(Reader::*)(const char*, const float&)const)&Reader::Read,
      (std::tuple<bool, int>(Reader::*)(const char*, const int&)const)&Reader::Read,
      (std::tuple<bool, bool>(Reader::*)(const char*, const bool& )const)&Reader::Read,
      (std::tuple<bool, std::string>(Reader::*)(const char*, const std::string&)const)&Reader::Read,
      (std::tuple<bool, glm::vec2>(Reader::*)(const char*, const glm::vec2&)const)&Reader::Read,
      (std::tuple<bool, glm::vec3>(Reader::*)(const char*, const glm::vec3&)const)&Reader::Read,
      (std::tuple<bool, glm::vec4>(Reader::*)(const char*, const glm::vec4&)const)&Reader::Read,
      (std::tuple<bool, base::FRect>(Reader::*)(const char*, const base::FRect&)const)&Reader::Read,
      (std::tuple<bool, base::FPoint>(Reader::*)(const char*, const base::FPoint&)const)&Reader::Read,
      (std::tuple<bool, base::FPoint>(Reader::*)(const char*, const base::FPoint&)const)&Reader::Read,
      (std::tuple<bool, base::Color4f>(Reader::*)(const char*, const base::Color4f&)const)&Reader::Read);
    reader["HasValue"]     = &Reader::HasValue;
    reader["HasChunk"]     = &Reader::HasChunk;
    reader["IsEmpty"]      = &Reader::IsEmpty;
    reader["GetNumChunks"] = &Reader::GetNumChunks;
    reader["GetReadChunk"] = [](const Reader& reader, const char* name, unsigned index) {
        const auto chunks = reader.GetNumChunks(name);
        if (index >= chunks)
            throw GameError("data reader chunk index out of bounds.");
        return reader.GetReadChunk(name, index);
    };
}

} // namespace

namespace engine
{

void BindData(sol::state& L)
{
    auto data = L.create_named_table("data");
    auto reader = data.new_usertype<data::Reader>("Reader");
    BindDataReaderInterface<data::Reader>(reader);

    auto writer = data.new_usertype<data::Writer>("Writer");
    BindDataWriterInterface<data::Writer>(writer);

    // there's no automatic understanding that the JsonObject *is-a* reader/writer
    // and provides those methods. Thus the reader/writer methods must be bound on the
    // JsonObject's usertype as well.
    auto json = data.new_usertype<data::JsonObject>("JsonObject",sol::constructors<data::JsonObject()>());
    BindDataReaderInterface<data::JsonObject>(json);
    BindDataWriterInterface<data::JsonObject>(json);
    json["ParseString"] = sol::overload(
            (std::tuple<bool, std::string>(data::JsonObject::*)(const std::string&))&data::JsonObject::ParseString,
            (std::tuple<bool, std::string>(data::JsonObject::*)(const char*, size_t))&data::JsonObject::ParseString);
    json["ToString"] = &data::JsonObject::ToString;

    data["ParseJsonString"] = sol::overload(
            [](const std::string& json) {
                auto ret = std::make_unique<data::JsonObject>();
                auto [ok, error] = ret->ParseString(json);
                if (!ok)
                    ret.reset();
                return std::make_tuple(std::move(ret), std::move(error));
            },
            [](const char* json, size_t len) {
                auto ret = std::make_unique<data::JsonObject>();
                auto [ok, error] = ret->ParseString(json, len);
                if (!ok)
                    ret.reset();
                return std::make_tuple(std::move(ret), std::move(error));
            });
    data["WriteJsonFile"] = [](const data::JsonObject& json, const std::string& file) {
        return data::WriteJsonFile(json, file);
    };
    data["ReadJsonFile"] = [](const std::string& file) {
        return data::ReadJsonFile(file);
    };
    data["CreateWriter"] = [](const std::string& format) {
        std::unique_ptr<data::Writer> ret;
        if (format == "JSON")
            ret.reset(new data::JsonObject);
        return ret;
    };
    // overload this when/if there are different data formats
    data["WriteFile"] = data::WriteFile;
    data["ReadFile"] = [](const std::string& file) {
        const auto& upper = base::ToUpperUtf8(file);
        std::unique_ptr<data::Reader> ret;
        if (base::EndsWith(upper, ".JSON")) {
            auto [json, error] = data::ReadJsonFile(file);
            if (json) {
                ret = std::move(json);
                return std::make_tuple(std::move(ret), std::string(""));
            }
            return std::make_tuple(std::move(ret), std::move(error));
        }
        return std::make_tuple(std::move(ret), std::string("unsupported file type"));
    };
}

} // namespace
