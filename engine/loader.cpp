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
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <memory>

#if defined(POSIX_OS)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/mman.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <cerrno>
#  include <cstring> // for memcpy
#elif defined(WINDOWS_OS)
#  include <Windows.h>
#  include <Shlobj.h>
#  include <cstring>
#  undef MIN
#  undef MAX
#  undef min
#  undef max
#  undef ERROR
#  undef DEBUG
#endif

#include "base/logging.h"
#include "base/utility.h"
#include "data/json.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "game/entity.h"
#include "game/scene.h"
#include "game/tilemap.h"
#include "engine/loader.h"
#include "uikit/window.h"
#include "audio/graph.h"

namespace engine
{

bool LoadFileBufferFromDisk(const std::string& filename, std::vector<char>* buffer)
{
    auto in = base::OpenBinaryInputStream(filename);
    if (!in.is_open())
    {
        ERROR("Failed to open file for reading. [file='%1']", filename);
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto size = (std::size_t)in.tellg();
    in.seekg(0, std::ios::beg);
    buffer->resize(size);
    in.read(&(*buffer)[0], size);
    if ((std::size_t)in.gcount() != size)
    {
        ERROR("Failed to read all of file.[file='%1']", filename);
        return false;
    }

    DEBUG("Loaded file buffer. [file='%1', bytes=%2]", filename, buffer->size());
    return true;
}

class TilemapDataBuffer : public game::TilemapData
{
public:
    TilemapDataBuffer(const std::string& filename, bool read_only, std::vector<char>&& data)
      : mFileName(filename)
      , mReadonly(read_only)
      , mFileData(std::move(data))
    {}

    virtual void Write(const void* ptr, size_t bytes, size_t offset) override
    {
        ASSERT(offset + bytes <= mFileData.size());
        ASSERT(mReadonly == false);
        std::memcpy(&mFileData[offset], ptr, bytes);
    }
    virtual void Read(void* ptr, size_t bytes, size_t offset) const override
    {
        ASSERT(offset + bytes <= mFileData.size());
        std::memcpy(ptr, &mFileData[offset], bytes);
    }

    virtual size_t AppendChunk(size_t bytes) override
    {
        ASSERT(mReadonly == false);
        const auto offset = mFileData.size();
        mFileData.resize(offset + bytes);
        return offset;
    }
    virtual size_t GetByteCount() const override
    {
        return mFileData.size();
    }
    virtual void Resize(size_t bytes) override
    {
        ASSERT(mReadonly == false);
        mFileData.resize(bytes);
    }

    virtual void ClearChunk(const void* value, size_t value_size, size_t offset, size_t num_values) override
    {
        ASSERT(mReadonly == false);
        ASSERT(offset + value_size * num_values <= mFileData.size());

        for (size_t i=0; i<num_values; ++i)
        {
            const auto buffer_offset = offset + i * value_size;
            std::memcpy(&mFileData[buffer_offset], value, value_size);
        }
    }
private:
    const std::string mFileName;
    const bool mReadonly = false;
    std::vector<char> mFileData;
};

template<typename Interface>
class FileBuffer : public Interface
{
public:
    FileBuffer(const std::string& filename, std::vector<char>&& data)
        : mFileName(filename)
        , mFileData(std::move(data))
    {}
    virtual const void* GetData() const override
    {
        if (mFileData.empty())
            return nullptr;
        return &mFileData[0];
    }
    virtual std::size_t GetByteSize() const override
    { return mFileData.size(); }
    virtual std::string GetSourceName() const override
    { return mFileName; }
private:
    const std::string mFileName;
    const std::vector<char> mFileData;
};

using GameDataFileBuffer = FileBuffer<EngineData>;
using GraphicsFileBuffer = FileBuffer<gfx::Resource>;

#if defined(POSIX_OS)
class AudioFileMap : public audio::SourceStream
{
public:
    AudioFileMap(const std::string& filename)
      : mFileName(filename)
    {}
    ~AudioFileMap()
    {
        if (mBase)
            ASSERT(::munmap(mBase, mSize) == 0);
        if (mFile)
            ASSERT(::close(mFile) == 0);
    }
    virtual void Read(void* ptr, uint64_t offset, uint64_t bytes) const override
    {
        ASSERT(offset + bytes <= mSize);
        bytes = std::min(mSize - offset, bytes);
        const auto* base = static_cast<const char*>(mBase);
        std::memcpy(ptr, &base[offset], bytes);
    }
    virtual std::uint64_t GetSize() const override
    { return mSize; }
    virtual std::string GetName() const override
    { return mFileName; }

    bool Map()
    {
        ASSERT(mFile == 0);
        mFile = ::open(mFileName.c_str(), O_RDONLY);
        if (mFile == -1)
        {
            ERROR("Failed to open file. [file='%1', error='%2']", mFileName, strerror(errno));
            return false;
        }
        struct stat64 stat;
        if (fstat64(mFile, &stat))
        {
            ERROR("Failed to fstat64 file. [file='%1', error='%2']", mFileName, strerror(errno));
            return false;
        }
        mSize = stat.st_size;
        mBase = ::mmap(0, mSize, PROT_READ, MAP_SHARED, mFile, off_t(0));
        if (mBase == MAP_FAILED)
        {
            ERROR("Failed to mmap file. [file='%1', error='%2']", mFileName, strerror(errno));
            return false;
        }
        DEBUG("Mapped audio file successfully. [file='%1', size='%2']", mFileName, mSize);
        return true;
    }
private:
    const std::string mFileName;
    int mFile = 0;
    void* mBase = nullptr;
    std::uint64_t mSize = 0;
};
#elif defined(WINDOWS_OS)
std::string FormatError(DWORD error)
{
    LPSTR buffer = nullptr;
    const auto flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS;
    const auto lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    size_t size = FormatMessageA(flags, NULL, error, lang, (LPSTR)&buffer, 0, NULL);
    std::string ret(buffer, size);
    LocalFree(buffer);
    if (ret.back() == '\n')
        ret.pop_back();
    if (ret.back() == '\r')
        ret.pop_back();
    return ret;
}

std::string ErrorString()
{
    return FormatError(GetLastError());
}

class AudioFileMap : public audio::SourceStream
{
public:
    AudioFileMap(const std::string& filename)
        : mFileName(filename)
    {}
    ~AudioFileMap()
    {
        if (mBase != nullptr)
            ASSERT(UnmapViewOfFile(mBase) == TRUE);
        if (mMap != NULL)
            ASSERT(CloseHandle(mMap) == TRUE);
        if (mFile != INVALID_HANDLE_VALUE)
            ASSERT(CloseHandle(mFile) == TRUE);
    }
    virtual void Read(void* ptr, uint64_t offset, uint64_t bytes) const override
    {
        ASSERT(offset + bytes <= mSize);
        bytes = std::min(mSize - offset, bytes);
        const auto* base = static_cast<const char*>(mBase);
        std::memcpy(ptr, &base[offset], bytes);
    }
    virtual std::uint64_t GetSize() const override 
    {
        return mSize; 
    }
    virtual std::string GetName() const override
    {
        return mFileName;
    }
    bool Map()
    {
        const auto& str = base::FromUtf8(mFileName);
        mFile = CreateFile(
            str.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (mFile == INVALID_HANDLE_VALUE)
        {
            ERROR("Failed to open file. [file='%1', error='%2']", str, ErrorString());
            return false;
        }

        LARGE_INTEGER size;
        if (!GetFileSizeEx(mFile, &size))
        {
            ERROR("Failed to get file size. [file='%1', error='%2']", str, ErrorString());
            return false;
        }
        mSize = size.QuadPart;

        mMap = CreateFileMapping(
            mFile,
            NULL, // default security
            PAGE_READONLY,
            size.HighPart,
            size.LowPart,
            NULL);
        if (mMap == NULL)
        {
            ERROR("Failed to create file map. [file='%1', error='%2']", str, ErrorString());
            return false;
        }

        mBase = MapViewOfFile(mMap, FILE_MAP_READ, 0, 0, mSize);
        if (mBase == NULL)
        {
            ERROR("Failed to map view of file. [file='%1', error='%2']", str, ErrorString());
            return false;
        }
        DEBUG("Mapped audio file successfully. [file='%1', size=%2]", str, mSize);
        return true;
    }
private:
    const std::string mFileName;
    HANDLE mFile = INVALID_HANDLE_VALUE;
    HANDLE mMap = NULL;
    void* mBase = nullptr;
    std::uint64_t mSize = 0;
};
#endif

class AudioStream : public audio::SourceStream
{
public:
    AudioStream(const std::string& filename) : mFileName(filename)
    {}
    bool Open()
    {
        mStream = base::OpenBinaryInputStream(mFileName);
        if (!mStream.is_open())
        {
            ERROR("Failed to open file stream. [file='%1']", mFileName);
            return false;
        }
        mStream.seekg(0, std::ios::end);
        mSize = (std::uint64_t)mStream.tellg();
        mStream.seekg(0, std::ios::beg);
        DEBUG("Opened audio file stream. [file='%1' bytes=%2]", mFileName, mSize);
        return true;
    }
    virtual void Read(void* ptr, uint64_t offset, uint64_t bytes) const override
    {
        auto* stream = const_cast<std::ifstream*>(&mStream);
        stream->seekg(offset, std::ios::beg);
        stream->read((char*)ptr, bytes);
    }
    virtual std::uint64_t GetSize() const override
    { return mSize; }
    virtual std::string GetName() const override
    { return mFileName; }
private:
    const std::string mFileName;
    std::ifstream mStream;
    std::uint64_t mSize = 0;
};

class FileResourceLoaderImpl : public FileResourceLoader
{
public:
    // gfx::resource loader impl
    virtual gfx::ResourceHandle LoadResource(const std::string& uri) override
    {
        const auto& filename = ResolveURI(uri);
        auto it = mGraphicsFileBufferCache.find(filename);
        if (it != mGraphicsFileBufferCache.end())
            return it->second;

        std::vector<char> buffer;
        if (!LoadFileBuffer(filename, &buffer))
            return nullptr;

        auto buff = std::make_shared<GraphicsFileBuffer>(uri, std::move(buffer));
        mGraphicsFileBufferCache[filename] = buff;
        return buff;
    }
    // GameDataLoader impl
    virtual EngineDataHandle LoadEngineDataUri(const std::string& uri) const override
    {
        const auto& filename = ResolveURI(uri);
        auto it = mGameDataBufferCache.find(filename);
        if (it != mGameDataBufferCache.end())
            return it->second;

        std::vector<char> buffer;
        if (!LoadFileBuffer(filename, &buffer))
            return nullptr;

        auto buff = std::make_shared<GameDataFileBuffer>(uri, std::move(buffer));
        mGameDataBufferCache[filename] = buff;
        return buff;
    }
    virtual EngineDataHandle LoadEngineDataFile(const std::string& filename) const override
    {
        // expect this to be a path relative to the content path
        // this loading function is only used to load the Lua files
        // which don't yet proper resource URIs. When that is fixed
        // this function can go away!
        const auto& file = base::JoinPath(mContentPath, filename);

        auto it = mGameDataBufferCache.find(file);
        if (it != mGameDataBufferCache.end())
            return it->second;

        std::vector<char> buffer;
        if (!LoadFileBuffer(file, &buffer))
            return nullptr;

        auto buff = std::make_shared<GameDataFileBuffer>(file, std::move(buffer));
        mGameDataBufferCache[file] = buff;
        return buff;
    }
    virtual EngineDataHandle LoadEngineDataId(const std::string& id) const override
    {
        if (const auto* uri = base::SafeFind(mObjectIdUriMap, id))
        {
            const auto& filename = ResolveURI(*uri);
            auto it = mGameDataBufferCache.find(filename);
            if (it != mGameDataBufferCache.end())
                return it->second;

            std::vector<char> buffer;
            if (!LoadFileBuffer(filename, &buffer))
                return nullptr;

            auto buff = std::make_shared<GameDataFileBuffer>(*uri, std::move(buffer));
            mGameDataBufferCache[filename] = buff;
            return buff;
        }
        ERROR("No URI mapping for engine data. [id='%1']", id);
        return nullptr;
    }

    // audio::Loader impl
    virtual audio::SourceStreamHandle OpenAudioStream(const std::string& uri,
        AudioIOStrategy strategy, bool enable_file_caching) const override
    {
        const auto& filename = ResolveURI(uri);
        if (enable_file_caching)
        {
            auto it = mAudioStreamCache.find(filename);
            if (it != mAudioStreamCache.end())
                return it->second;
        }
        // if the requested IO strategy is default then see what
        // default actually means based on the configured setting.
        if (strategy == AudioIOStrategy::Default)
        {
            if (mDefaultAudioIO == DefaultAudioIOStrategy::Automatic)
                strategy = AudioIOStrategy::Automatic;
            else if (mDefaultAudioIO == DefaultAudioIOStrategy::Memmap)
                strategy = AudioIOStrategy::Memmap;
            else if (mDefaultAudioIO == DefaultAudioIOStrategy::Buffer)
                strategy = AudioIOStrategy::Buffer;
            else if (mDefaultAudioIO == DefaultAudioIOStrategy::Stream)
                strategy = AudioIOStrategy ::Stream;
            else BUG("Unhandled default audio IO strategy.");
        }
#if defined(WEBASSEMBLY)
        if (strategy == AudioIOStrategy::Automatic)
        {
            auto map = std::make_shared<AudioFileMap>(filename);
            if (!map->Map())
                return nullptr;
            if (enable_file_caching)
                mAudioStreamCache[filename] = map;
            return map;
        }
#elif defined(POSIX_OS)
      if (strategy == AudioIOStrategy::Automatic)
      {
          auto stream = std::make_shared<AudioStream>(filename);
          if (!stream->Open())
              return nullptr;
          return stream;
      }
#elif defined(WINDOWS_OS)
      if (strategy == AudioIOStrategy::Automatic)
      {
          auto stream = std::make_shared<AudioStream>(filename);
          if (!stream->Open())
              return nullptr;
          return stream;
      }
#endif

        if (strategy == AudioIOStrategy::Memmap)
        {
            auto map = std::make_shared<AudioFileMap>(filename);
            if (!map->Map())
                return nullptr;
            if (enable_file_caching)
                mAudioStreamCache[filename] = map;
            return map;
        }

        // open using ifstream. since the stream is stateful (read position)
        // sharing cannot be done right now.
        if (strategy == AudioIOStrategy::Stream)
        {
            auto stream = std::make_shared<AudioStream>(filename);
            if (!stream->Open())
                return nullptr;
            return stream;
        }

        // default implementation
        auto ret = audio::OpenFileStream(filename);
        if (ret && enable_file_caching)
            mAudioStreamCache[filename] = ret;
        return ret;
    }

    // game::Loader impl
    virtual game::TilemapDataHandle LoadTilemapData(const game::Loader::TilemapDataDesc& desc) const override
    {
        const auto& filename = ResolveURI(desc.uri);

        std::vector<char> buffer;
        if (!LoadFileBuffer(filename, &buffer))
            return nullptr;

        return std::make_shared<TilemapDataBuffer>(desc.uri, desc.read_only, std::move(buffer));
    }

    // FileResourceLoader impl
    virtual bool LoadResourceLoadingInfo(const data::Reader& data) override
    {
        for (unsigned i=0; i<data.GetNumChunks("scripts"); ++i)
        {
            std::string id;
            std::string uri;
            const auto& chunk = data.GetReadChunk("scripts", i);
            chunk->Read("id", &id);
            chunk->Read("uri", &uri);
            mObjectIdUriMap[std::move(id)] = std::move(uri);
        }
        for (unsigned i=0; i<data.GetNumChunks("data_files"); ++i)
        {
            std::string id;
            std::string uri;
            const auto& chunk = data.GetReadChunk("data_files", i);
            chunk->Read("id", &id);
            chunk->Read("uri", &uri);
            mObjectIdUriMap[std::move(id)] = std::move(uri);
        }
        return true;
    }

    virtual void SetDefaultAudioIOStrategy(DefaultAudioIOStrategy strategy) override
    { mDefaultAudioIO = strategy; }
    virtual void SetApplicationPath(const std::string& path) override
    { mApplicationPath = path; }
    virtual void SetContentPath(const std::string& path) override
    { mContentPath = path; }
    virtual void PreloadFiles() override
    {
        DEBUG("Preloading file buffers.");
        const char* dirs[] = {
          "fonts", "lua", "textures", "ui/style", "shaders/es2"
        };
        size_t bytes_loaded = 0;
        for (size_t i=0; i<base::ArraySize(dirs); ++i)
        {
            const auto& dir = dirs[i];
            const auto& path = base::JoinPath(mContentPath, dir);
            if (!base::FileExists(path))
                continue;
            for (const auto& entry : std::filesystem::directory_iterator(path))
            {
                if (!entry.is_regular_file())
                    continue;
                const auto& file = entry.path().generic_u8string();
                std::vector<char> buffer;
                if (!LoadFileBufferFromDisk(file, &buffer))
                    continue;
                const auto size = buffer.size();
                bytes_loaded += buffer.size();
                mPreloadedFiles[file] = std::move(buffer);
            }
        }
    }
private:
    bool LoadFileBuffer(const std::string& filename, std::vector<char>* buffer) const
    {
        if (mPreloadedFiles.empty())
            return LoadFileBufferFromDisk(filename, buffer);

        auto it = mPreloadedFiles.find(filename);
        if (it == mPreloadedFiles.end())
        {
            WARN("Missed preloaded file buffer entry. [file='%1']", filename);
            return LoadFileBufferFromDisk(filename, buffer);
        }
        // ditch the preloaded buffer since it'll be now cached in a higher
        // level of a buffer object, i.e. graphics buffer, audio buffer etc.
        *buffer = std::move(it->second);
        mPreloadedFiles.erase(it);
        return true;
    }

    std::string ResolveURI(const std::string& URI) const
    {
        auto it = mUriCache.find(URI);
        if (it != mUriCache.end())
            return it->second;

        // note that there might still be some resource URIs
        // with app:// in them even after packing. An example
        // is a font reference in the default UI style.json file.
        // the packing procedure in workspace puts these together
        // with the rest of the game data so for simply use the
        // content path for the app:// path as well.

        std::string str = URI;
        if (base::StartsWith(str, "pck://"))
            str = mContentPath + "/" + str.substr(6);
        else if (base::StartsWith(str, "app://"))
            str = mContentPath + "/" + str.substr(6); // see the comment above about app://
        else if (base::StartsWith(str, "fs://"))
            str = str.substr(5);
        else WARN("Unmapped resource URI. [uri='%1']", URI);

        DEBUG("New resource URI mapping. [uri='%1', file='%2']", URI, str);
        mUriCache[URI] = str;
        return str;
    }
private:
    // transient stash of files that have been preloaded
    mutable std::unordered_map<std::string, std::vector<char>> mPreloadedFiles;
    // cache of URIs that have been resolved to file
    // names already.
    mutable std::unordered_map<std::string, std::string> mUriCache;
    // cache of graphics file buffers that have already been loaded.
    mutable std::unordered_map<std::string,
        std::shared_ptr<const GraphicsFileBuffer>> mGraphicsFileBufferCache;
    mutable std::unordered_map<std::string,
        std::shared_ptr<const GameDataFileBuffer>> mGameDataBufferCache;
    mutable std::unordered_map<std::string,
            std::shared_ptr<const audio::SourceStream>> mAudioStreamCache;
    DefaultAudioIOStrategy mDefaultAudioIO = DefaultAudioIOStrategy::Automatic;
    // the root of the resource dir against which to resolve the resource URIs.
    std::string mContentPath;
    std::string mApplicationPath;
    // Mapping from IDs to URIS. This happens with scripts and data objects
    // that are referenced by an ID by some higher level object such as an
    // entity or tilemap. Originally in the editor the ID is the ID of a
    // workspace resource object that then contains the URI that maps to the
    // actual file that contains the data. In this loader implementation
    // we have no use for the actual object so the whole thing can be
    // simplified to juse ID->URI mapping.
    std::unordered_map<std::string, std::string> mObjectIdUriMap;
};

// static
std::unique_ptr<FileResourceLoader> FileResourceLoader::Create()
{ return std::make_unique<FileResourceLoaderImpl>(); }

class ContentLoaderImpl : public JsonFileClassLoader
{
public:
    ContentLoaderImpl();

    // ClassLibrary implementation.
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassById(const std::string& id) const  override;
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassByName(const std::string& name) const override;
    virtual ClassHandle<const uik::Window> FindUIByName(const std::string& name) const override;
    virtual ClassHandle<const uik::Window> FindUIById(const std::string& id) const override;
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassByName(const std::string& name) const override;
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& id) const override;
    virtual ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& id) const override;
    virtual ClassHandle<const game::EntityClass> FindEntityClassByName(const std::string& name) const override;
    virtual ClassHandle<const game::EntityClass> FindEntityClassById(const std::string& id) const override;
    virtual ClassHandle<const game::SceneClass> FindSceneClassByName(const std::string& name) const override;
    virtual ClassHandle<const game::SceneClass> FindSceneClassById(const std::string& id) const override;
    virtual ClassHandle<const game::TilemapClass> FindTilemapClassById(const std::string& id) const override;
    // ContentLoader impl
    virtual bool LoadClasses(const data::Reader& data) override;
private:
    // These are the material types that have been loaded
    // from the resource file.
    std::unordered_map<std::string, std::shared_ptr<gfx::MaterialClass>> mMaterials;
    // These are the drawable types that have been loaded
    // from the resource file.
    std::unordered_map<std::string, std::shared_ptr<gfx::DrawableClass>> mDrawables;
    // These are the entities that have been loaded from
    // the resource file.
    std::unordered_map<std::string, std::shared_ptr<game::EntityClass>> mEntities;
    // These are the scenes that have been loaded from
    // the resource file.
    std::unordered_map<std::string, std::shared_ptr<game::SceneClass>> mScenes;
    // name table maps entity names to ids.
    std::unordered_map<std::string, std::string> mEntityNameTable;
    // name table maps scene names to ids.
    std::unordered_map<std::string, std::string> mSceneNameTable;
    // UI objects (windows)
    std::unordered_map<std::string, std::shared_ptr<uik::Window>> mWindows;
    // Audio graphs
    std::unordered_map<std::string, std::shared_ptr<audio::GraphClass>> mAudioGraphs;
    // Tilemaps
    std::unordered_map<std::string, std::shared_ptr<game::TilemapClass>> mMaps;
};

ContentLoaderImpl::ContentLoaderImpl()
{
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        auto ret = std::make_shared<gfx::ColorClass>(gfx::MaterialClass::Type::Color, "_" + color_name);
        ret->SetBaseColor(val);
        ret->SetName("_" + color_name);
        ret->SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
        mMaterials["_" + color_name] = ret;
    }

    // these are the current primitive cases that are not packed as part of the resources
    mDrawables["_rect"]               = std::make_shared<gfx::RectangleClass>("_rect");
    mDrawables["_isosceles_triangle"] = std::make_shared<gfx::IsoscelesTriangleClass>("_isosceles_triangle");
    mDrawables["_right_triangle"]     = std::make_shared<gfx::RightTriangleClass>("_right_triangle");
    mDrawables["_capsule"]            = std::make_shared<gfx::CapsuleClass>("_capsule");
    mDrawables["_circle"]             = std::make_shared<gfx::CircleClass>("_circle");
    mDrawables["_semi_circle"]        = std::make_shared<gfx::SemiCircleClass>("_semi_circle");
    mDrawables["_round_rect"]         = std::make_shared<gfx::RoundRectangleClass>("_round_rect", "", 0.05f);
    mDrawables["_trapezoid"]          = std::make_shared<gfx::TrapezoidClass>("_trapezoid");
    mDrawables["_parallelogram"]      = std::make_shared<gfx::ParallelogramClass>("_parallelogram");
    mDrawables["_arrow_cursor"]       = std::make_shared<gfx::ArrowCursorClass>("_arrow_cursor");
    mDrawables["_block_cursor"]       = std::make_shared<gfx::BlockCursorClass>("_block_cursor");
    mDrawables["_cone"]               = std::make_shared<gfx::ConeClass>("_cone", "", 100);
    mDrawables["_cube"]               = std::make_shared<gfx::CubeClass>("_cube");
    mDrawables["_cylinder"]           = std::make_shared<gfx::CylinderClass>("_cylinder", "", 100);
    mDrawables["_pyramid"]            = std::make_shared<gfx::PyramidClass>("_pyramid");
    mDrawables["_sphere"]             = std::make_shared<gfx::SphereClass>("_sphere", "", 100);
}

ClassHandle<const audio::GraphClass> ContentLoaderImpl::FindAudioGraphClassById(const std::string& id) const
{
    auto it = mAudioGraphs.find(id);
    if (it == mAudioGraphs.end())
        return nullptr;
    return it->second;
}
ClassHandle<const audio::GraphClass> ContentLoaderImpl::FindAudioGraphClassByName(const std::string& name) const
{
    for (auto it=mAudioGraphs.begin(); it != mAudioGraphs.end(); ++it)
    {
        auto& graph = it->second;
        if (graph->GetName() == name)
            return graph;
    }
    return nullptr;
}

ClassHandle<const uik::Window> ContentLoaderImpl::FindUIByName(const std::string& name) const
{
    for (auto it=mWindows.begin(); it != mWindows.end(); ++it)
    {
        auto& win = it->second;
        if (win->GetName() == name)
            return win;
    }
    return nullptr;
}
ClassHandle<const uik::Window> ContentLoaderImpl::FindUIById(const std::string& id) const
{
    auto it = mWindows.find(id);
    if (it == mWindows.end())
        return nullptr;
    return it->second;
}

ClassHandle<const gfx::MaterialClass> ContentLoaderImpl::FindMaterialClassByName(const std::string& name) const
{
    for (auto& [key, klass] : mMaterials)
    {
        if (klass->GetName() == name)
            return klass;
    }
    return nullptr;
}

ClassHandle<const gfx::MaterialClass> ContentLoaderImpl::FindMaterialClassById(const std::string& id) const
{
    auto it = mMaterials.find(id);
    if (it != std::end(mMaterials))
        return it->second;
    return nullptr;
}

ClassHandle<const gfx::DrawableClass> ContentLoaderImpl::FindDrawableClassById(const std::string& id) const
{
    auto it = mDrawables.find(id);
    if (it != std::end(mDrawables))
        return it->second;
    return nullptr;
}


template<typename Interface, typename Implementation = Interface>
bool LoadContent(const data::Reader& data, const char* type,
    std::unordered_map<std::string, std::shared_ptr<Interface>>& out,
    std::unordered_map<std::string, std::string>* namemap)
{
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        const auto& chunk = data.GetReadChunk(type, i);
        std::string id;
        std::string name;
        chunk->Read("resource_id", &id);
        chunk->Read("resource_name", &name);
        Implementation klass;
        if (!klass.FromJson(*chunk))
        {
            ERROR("Failed to load game class. [type='%1', name='%2]", type, name);
            return false;
        }

        out[id] = std::make_shared<Implementation>(std::move(klass));
        if (namemap)
            (*namemap)[name] = id;
        DEBUG("Loaded new game class. [type='%1', name='%2']", type, name);
    }
    return true;
}

bool LoadMaterials(const data::Reader& data, const char* type,
                 std::unordered_map<std::string, std::shared_ptr<gfx::MaterialClass>>& out,
                 std::unordered_map<std::string, std::string>* namemap)
{
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        const auto& chunk = data.GetReadChunk(type, i);
        std::string id;
        std::string name;
        chunk->Read("resource_id", &id);
        chunk->Read("resource_name", &name);
        auto ret = gfx::MaterialClass::ClassFromJson(*chunk);
        if (!ret)
        {
            ERROR("Failed to load game class. [type='%1', name='%2'].", type, name);
            return false;
        }

        out[id] = std::move(ret);
        if (namemap)
            (*namemap)[name] = id;
        DEBUG("Loaded new game class. [type='%1', name='%2']", type, name);
    }
    return true;
}

bool ContentLoaderImpl::LoadClasses(const data::Reader& data)
{
    if (!LoadMaterials(data, "materials", mMaterials, nullptr))
        return false;
    if (!LoadContent<gfx::DrawableClass, gfx::ParticleEngineClass>(data, "particles", mDrawables, nullptr))
        return false;
   if (!LoadContent<gfx::DrawableClass, gfx::PolygonMeshClass>(data, "shapes", mDrawables, nullptr))
       return false;
   if (!LoadContent<game::EntityClass>(data, "entities", mEntities, &mEntityNameTable))
       return false;
   if (!LoadContent<game::SceneClass>(data, "scenes", mScenes, &mSceneNameTable))
       return false;
   if (!LoadContent<uik::Window>(data, "uis", mWindows, nullptr))
       return false;
   if (!LoadContent<audio::GraphClass>(data, "audio_graphs", mAudioGraphs, nullptr))
        return false;
   if (!LoadContent<game::TilemapClass>(data, "tilemaps", mMaps, nullptr))
       return false;

    // need to resolve the entity references.
    for (auto& p : mScenes)
    {
        auto& scene = p.second;
        for (size_t i=0; i<scene->GetNumNodes(); ++i)
        {
            auto& node = scene->GetPlacement(i);
            auto klass = FindEntityClassById(node.GetEntityId());
            if (!klass)
            {
                const auto& scene_name = mSceneNameTable[scene->GetId()];
                const auto& node_name  = node.GetName();
                const auto& node_entity_id = node.GetEntityId();
                ERROR("Scene node refers to entity that is not found. [scene='%1', node='%2', entity=%3]",
                      scene_name, node_name, node_entity_id);
                return false;
            }
            node.SetEntity(klass);
        }
    }
    return true;
}

ClassHandle<const game::EntityClass> ContentLoaderImpl::FindEntityClassByName(const std::string& name) const
{
    auto it = mEntityNameTable.find(name);
    if (it != mEntityNameTable.end())
        return FindEntityClassById(it->second);

    return nullptr;
}
ClassHandle<const game::EntityClass> ContentLoaderImpl::FindEntityClassById(const std::string& id) const
{
    auto it = mEntities.find(id);
    if (it != std::end(mEntities))
        return it->second;

    return nullptr;
}

ClassHandle<const game::SceneClass> ContentLoaderImpl::FindSceneClassByName(const std::string& name) const
{
    auto it = mSceneNameTable.find(name);
    if (it != mSceneNameTable.end())
        return FindSceneClassById(it->second);

    return nullptr;
}
ClassHandle<const game::SceneClass> ContentLoaderImpl::FindSceneClassById(const std::string& id) const
{
    auto it = mScenes.find(id);
    if (it != mScenes.end())
        return it->second;

    return nullptr;
}
ClassHandle<const game::TilemapClass> ContentLoaderImpl::FindTilemapClassById(const std::string& id) const
{
    auto it = mMaps.find(id);
    if (it != mMaps.end())
        return it->second;

    return nullptr;
}

// static
std::unique_ptr<JsonFileClassLoader> JsonFileClassLoader::Create()
{ return std::make_unique<ContentLoaderImpl>(); }

bool JsonFileClassLoader::LoadClassesFromFile(const std::string& file)
{
    data::JsonFile json;
    const auto [success, error] = json.Load(file);
    if (!success)
    {
        ERROR("Failed to load game content from file. [file=%1', error='%2']", file, error);
        return false;
    }
    return this->LoadClasses(json.GetRootObject());
}

} // namespace
