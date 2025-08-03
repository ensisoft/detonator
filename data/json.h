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

#include "data/chunk.h"
#include "data/reader.h"
#include "data/writer.h"

#include <memory>
#include <tuple>

namespace data
{
    class JsonObject : public Chunk, public Reader, public Writer
    {
    public:
        explicit JsonObject(const nlohmann::json& json);
        explicit JsonObject(nlohmann::json&& json);
        explicit JsonObject(std::shared_ptr<nlohmann::json> json) noexcept;
        JsonObject(const JsonObject&) = delete;
        JsonObject();
       ~JsonObject() override;

        // reader interface impl
        std::unique_ptr<Reader> GetReadChunk(const char* name) const override;
        std::unique_ptr<Reader> GetReadChunk(const char* name, unsigned index) const override;
        std::unique_ptr<Chunk> GetChunk(const char* name) const override;
        std::unique_ptr<Chunk> GetChunk(const char* name, unsigned index) const override;
        bool Read(const char* name, double* out) const override;
        bool Read(const char* name, float* out) const override;
        bool Read(const char* name, int* out) const override;
        bool Read(const char* name, unsigned* out) const override;
        bool Read(const char* name, bool* out) const override;
        bool Read(const char* name, std::string* out) const override;
        // array read for primitive items.
        bool Read(const char* name, unsigned index, double* out) const override;
        bool Read(const char* name, unsigned index, float* out) const override;
        bool Read(const char* name, unsigned index, int* out) const override;
        bool Read(const char* name, unsigned index, unsigned* out) const override;
        bool Read(const char* name, unsigned index, bool* out) const override;
        bool Read(const char* name, unsigned index, std::string* out) const override;
        bool Read(const char* name, unsigned index, glm::vec2* out) const override;

        bool Read(const char* name, glm::vec2* out) const override;
        bool Read(const char* name, glm::vec3* out) const override;
        bool Read(const char* name, glm::vec4* out) const override;
        bool Read(const char* name, base::FDegrees* degrees) const override;
        bool Read(const char* name, base::FRadians* radians) const override;
        bool Read(const char* name, base::FRect* rect) const override;
        bool Read(const char* name, base::FPoint* point) const override;
        bool Read(const char* name, base::FSize* point) const override;
        bool Read(const char* name, base::Color4f* color) const override;
        bool Read(const char* name, base::Rotator* rotator) const override;
        bool HasValue(const char* name) const override;
        bool HasChunk(const char* name) const override;
        bool HasArray(const char* name) const override;
        bool IsEmpty() const override;
        unsigned GetNumItems(const char* array) const override;
        unsigned GetNumChunks(const char* name) const override;

        // writer interface impl.
        std::unique_ptr<Chunk> NewChunk() const override;
        std::unique_ptr<Writer> NewWriteChunk() const override;
        void Write(const char* name, int value) override;
        void Write(const char* name, unsigned value) override;
        void Write(const char* name, double value) override;
        void Write(const char* name, float value) override;
        void Write(const char* name, bool value) override;
        void Write(const char* name, const char* value) override;
        void Write(const char* name, const std::string& value) override;
        void Write(const char* name, const glm::vec2& value) override;
        void Write(const char* name, const glm::vec3& value) override;
        void Write(const char* name, const glm::vec4& value) override;
        void Write(const char* name, const base::FDegrees& degrees) override;
        void Write(const char* name, const base::FRadians& radians) override;
        void Write(const char* name, const base::FRect& value) override;
        void Write(const char* name, const base::FPoint& value) override;
        void Write(const char* name, const base::FSize& value) override;
        void Write(const char* name, const base::Color4f& value) override;
        void Write(const char* name, const base::Rotator& value) override;
        void Write(const char* name, const Writer& chunk) override;
        void Write(const char* name, const Chunk& chunk) override;
        void Write(const char* name, std::unique_ptr<Writer> chunk) override;
        void Write(const char* name, std::unique_ptr<Chunk> chunk) override;
        void Write(const char* name, const int* array, size_t size) override;
        void Write(const char* name, const unsigned* array, size_t size) override;
        void Write(const char* name, const double* array, size_t size) override;
        void Write(const char* name, const float* array, size_t size) override;
        void Write(const char* name, const bool* array, size_t size) override;
        void Write(const char* name, const char* const * array, size_t size) override;
        void Write(const char* name, const std::string* array, size_t size) override;
        void Write(const char* name, const glm::vec2* array, size_t size) override;
        void AppendChunk(const char* name, const Writer& chunk) override;
        void AppendChunk(const char* name, const Chunk& chunk) override;
        void AppendChunk(const char* name, std::unique_ptr<Writer> chunk) override;
        void AppendChunk(const char* name, std::unique_ptr<Chunk> chunk) override;

        // bring the template helpers into scope when using this type.
        using Writer::Write;
        using Reader::Read;

        // From Chunk API
        Writer* GetWriter() noexcept override
        { return this; }
        const Reader* GetReader() const noexcept override
        { return this; }

        Chunk* GetChunkFromWriter() noexcept override
        { return this; }
        Chunk* GetChunkFromReader() noexcept override
        { return this; }
        const Chunk* GetChunkFromWriter() const noexcept override
        { return this; }
        const Chunk* GetChunkFromReader() const noexcept override
        { return this; }

        void OverwriteChunk(const char* name, std::unique_ptr<Chunk> chunk) override;
        void OverwriteChunk(const char* name, std::unique_ptr<Chunk> chunk, unsigned index) override;
        bool Dump(IODevice& device) const override;

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
