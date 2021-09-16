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

#include "data/writer.h"
#include "data/reader.h"
#include "engine/state.h"

namespace engine
{

void KeyValueStore::Persist(data::Writer& writer) const
{
    for (const auto& pair : mValues)
    {
        const auto& key = pair.first;
        const auto& val = pair.second;
        std::visit([&key, &writer](const auto& variant_value) {
            auto chunk = writer.NewWriteChunk();
            chunk->Write("key", key.c_str());
            chunk->Write("val", variant_value);
            writer.AppendChunk("values", std::move(chunk));
        }, val);
    }
}

template<typename T>
bool ReadValue(const data::Reader& data, KeyValueStore::Value& v)
{
    T value;
    if (!data.Read("val", &value))
        return false;
    v = value;
    return true;
}

bool KeyValueStore::Restore(const data::Reader& reader)
{
    for (unsigned i=0; i<reader.GetNumChunks("values"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("values", i);
        std::string key;
        if (!chunk->Read("key", &key))
            return false;
        Value val;
        // todo: is there a better way to do this?
        // note: the order of vec2/3/4 reading is important since
        // vec4 parses as vec3/2 and vec3 parses as vec2.
        // this should be fixed in data/json.cpp
        if (ReadValue<bool>(*chunk, val) ||
            ReadValue<int>(*chunk, val) ||
            ReadValue<float>(*chunk, val) ||
            ReadValue<glm::vec4>(*chunk, val) ||
            ReadValue<glm::vec3>(*chunk, val) ||
            ReadValue<glm::vec2>(*chunk, val) ||
            ReadValue<std::string>(*chunk, val) ||
            ReadValue<base::FRect>(*chunk, val) ||
            ReadValue<base::FPoint>(*chunk, val) ||
            ReadValue<base::FSize>(*chunk, val) ||
            ReadValue<base::Color4f>(*chunk, val))
        {
            mValues[std::move(key)] = std::move(val);
        } else return false;
    }
    return true;
}
} // namespace