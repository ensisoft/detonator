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

#include "warnpush.h"
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include "data/reader.h"
#include "data/writer.h"

#include <memory>
#include <tuple>

namespace data
{
    class JsonObject : public Reader,
                       public Writer
    {
    public:
        JsonObject(const nlohmann::json& json);
        JsonObject(nlohmann::json&& json);
        JsonObject(std::shared_ptr<nlohmann::json> json);
        JsonObject(const JsonObject&) = delete;
        JsonObject();
       ~JsonObject();

        // reader interface impl
        virtual std::unique_ptr<Reader> GetReadChunk(const char* name) const override;
        virtual std::unique_ptr<Reader> GetReadChunk(const char* name, unsigned index) const override;
        virtual bool Read(const char* name, double* out) const override;
        virtual bool Read(const char* name, float* out) const override;
        virtual bool Read(const char* name, int* out) const override;
        virtual bool Read(const char* name, unsigned* out) const override;
        virtual bool Read(const char* name, bool* out) const override;
        virtual bool Read(const char* name, std::string* out) const override;
        // array read for primitive items.
        virtual bool Read(const char* name, unsigned index, double* out) const override;
        virtual bool Read(const char* name, unsigned index, float* out) const override;
        virtual bool Read(const char* name, unsigned index, int* out) const override;
        virtual bool Read(const char* name, unsigned index, unsigned* out) const override;
        virtual bool Read(const char* name, unsigned index, bool* out) const override;
        virtual bool Read(const char* name, unsigned index, std::string* out) const override;
        virtual bool Read(const char* name, glm::vec2* out) const override;
        virtual bool Read(const char* name, glm::vec3* out) const override;
        virtual bool Read(const char* name, glm::vec4* out) const override;
        virtual bool Read(const char* name, base::FRect* rect) const override;
        virtual bool Read(const char* name, base::FPoint* point) const override;
        virtual bool Read(const char* name, base::FSize* point) const override;
        virtual bool Read(const char* name, base::Color4f* color) const override;
        virtual bool Read(const char* name, base::Rotator* rotator) const override;
        virtual bool HasValue(const char* name) const override;
        virtual bool HasChunk(const char* name) const override;
        virtual bool HasArray(const char* name) const override;
        virtual bool IsEmpty() const override;
        virtual unsigned GetNumItems(const char* array) const override;
        virtual unsigned GetNumChunks(const char* name) const override;

        // writer interface impl.
        virtual std::unique_ptr<Writer> NewWriteChunk() const override;
        virtual void Write(const char* name, int value) override;
        virtual void Write(const char* name, unsigned value) override;
        virtual void Write(const char* name, double value) override;
        virtual void Write(const char* name, float value) override;
        virtual void Write(const char* name, bool value) override;
        virtual void Write(const char* name, const char* value) override;
        virtual void Write(const char* name, const std::string& value) override;
        virtual void Write(const char* name, const glm::vec2& value) override;
        virtual void Write(const char* name, const glm::vec3& value) override;
        virtual void Write(const char* name, const glm::vec4& value) override;
        virtual void Write(const char* name, const base::FRect& value) override;
        virtual void Write(const char* name, const base::FPoint& value) override;
        virtual void Write(const char* name, const base::FSize& value) override;
        virtual void Write(const char* name, const base::Color4f& value) override;
        virtual void Write(const char* name, const base::Rotator& value) override;
        virtual void Write(const char* name, const Writer& chunk) override;
        virtual void Write(const char* name, std::unique_ptr<Writer> chunk) override;
        virtual void Write(const char* name, const int* array, size_t size) override;
        virtual void Write(const char* name, const unsigned* array, size_t size) override;
        virtual void Write(const char* name, const double* array, size_t size) override;
        virtual void Write(const char* name, const float* array, size_t size) override;
        virtual void Write(const char* name, const bool* array, size_t size) override;
        virtual void Write(const char* name, const char* const * array, size_t size) override;
        virtual void Write(const char* name, const std::string* array, size_t size) override;
        virtual void AppendChunk(const char* name, const Writer& chunk) override;
        virtual void AppendChunk(const char* name, std::unique_ptr<Writer> chunk) override;

        virtual bool Dump(IODevice& device) const override;

        // bring the template helpers into scope when using this type.
        using Writer::Write;
        using Reader::Read;

        JsonObject& operator=(const JsonObject&) = delete;

        std::shared_ptr<nlohmann::json> GetJson()
        { return mJson; }
        std::shared_ptr<const nlohmann::json> GetJson() const
        { return mJson; }
        std::tuple<bool, std::string> ParseString(const std::string& str);
        std::tuple<bool, std::string> ParseString(const char* str, size_t len);
        std::string ToString() const;
    private:
        template<typename PrimitiveType>
        void write_array(const char* name, const PrimitiveType* array, size_t size);
        template<typename PrimitiveType>
        bool read_array(const char* name, unsigned index, PrimitiveType* out) const;
    private:
        std::shared_ptr<nlohmann::json> mJson;
    };

    // Helper class to load and save JSON data from a file.
    class JsonFile
    {
    public:
        // Open a JSON file immediately.
        // On any error an exception will get thrown.
        JsonFile(const std::string& file);
        JsonFile(const JsonFile&) = delete;
        // Create an unopened JsonFile object.
        // Load can later be used to load the contents
        // of a JSON file.
        JsonFile();
       ~JsonFile();
        // Try to load the contents of the given file.
        // On error returns false and a description of the error.
        // On success returns true and an empty string.
        std::tuple<bool, std::string> Load(const std::string& file);
        // Try to save the contents into the given file.
        // On error returns false and a description of the error.
        // On success returns true and en empty string.
        std::tuple<bool, std::string> Save(const std::string& file) const;
        // Get the Json object for read/write access to the underlying
        // JSON data object.
        JsonObject GetRootObject();
        // Set a new JSON object as the underlying JSON data object.
        void SetRootObject(JsonObject& object);
        JsonFile& operator=(const JsonFile&) = delete;
    private:
        std::shared_ptr<nlohmann::json> mJson;
    };

    std::tuple<std::unique_ptr<JsonObject>, std::string> ReadJsonFile(const std::string& file);
    std::tuple<bool, std::string> WriteJsonFile(const JsonObject& json, const std::string& file);

} // namespace
