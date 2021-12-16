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

#include "base/logging.h"
#include "base/utility.h"
#include "data/json.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "game/entity.h"
#include "game/scene.h"
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
    virtual std::size_t GetSize() const override
    { return mFileData.size(); }
    virtual std::string GetName() const /*override*/
    { return mFileName; }
private:
    const std::string mFileName;
    const std::vector<char> mFileData;
};

using GameDataFileBuffer = FileBuffer<GameData>;
using GraphicsFileBuffer = FileBuffer<gfx::Resource>;
using AudioFileBuffer    = FileBuffer<audio::SourceBuffer>;

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
    virtual GameDataHandle LoadGameData(const std::string& uri) const override
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
    virtual GameDataHandle LoadGameDataFromFile(const std::string& filename) const override
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
    // audio::Loader impl
    virtual std::ifstream OpenAudioStream(const std::string& uri) const override
    {
        const auto& filename = ResolveURI(uri);
        auto stream = base::OpenBinaryInputStream(filename);
        if (!stream.is_open())
        {
            ERROR("Failed to open audio stream. [file='%1']", filename);
        }
        return stream;
    }
    virtual audio::SourceBufferHandle LoadAudioBuffer(const std::string& uri) const override
    {
        const auto& filename = ResolveURI(uri);
        auto it = mAudioFileBufferCache.find(filename);
        if (it != mAudioFileBufferCache.end())
            return it->second;

        std::vector<char> buffer;
        if (!LoadFileBuffer(filename, &buffer))
            return nullptr;

        auto buff = std::make_shared<AudioFileBuffer>(uri, std::move(buffer));
        mAudioFileBufferCache[filename] = buff;
        return buff;
    }

    // FileResourceLoader impl
    virtual void SetApplicationPath(const std::string& path) override
    { mApplicationPath = path; }
    virtual void SetContentPath(const std::string& path) override
    { mContentPath = path; }
    virtual void PreloadFiles() override
    {
        DEBUG("Preloading file buffers.");
        const char* dirs[] = {
          "audio", "fonts", "lua", "textures", "ui", "shaders/es2"
        };
        size_t bytes_loaded = 0;
        for (size_t i=0; i<base::ArraySize(dirs); ++i)
        {
            const auto& dir = dirs[i];
            const auto& path = base::JoinPath(mContentPath, dir);
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
            WARN("Missed preloaded file buffer. entry. [file='%1']", filename);
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
        std::shared_ptr<const AudioFileBuffer>> mAudioFileBufferCache;
    // the root of the resource dir against which to resolve the resource URIs.
    std::string mContentPath;
    std::string mApplicationPath;
};

// static
std::unique_ptr<FileResourceLoader> FileResourceLoader::Create()
{ return std::make_unique<FileResourceLoaderImpl>(); }

class ContentLoaderImpl : public JsonFileClassLoader
{
public:
    // ClassLibrary implementation.
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassById(const std::string& id) const  override;
    virtual ClassHandle<const audio::GraphClass> FindAudioGraphClassByName(const std::string& name) const override;
    virtual ClassHandle<const uik::Window> FindUIByName(const std::string& name) const override;
    virtual ClassHandle<const uik::Window> FindUIById(const std::string& id) const override;
    virtual ClassHandle<const gfx::MaterialClass> FindMaterialClassById(const std::string& name) const override;
    virtual ClassHandle<const gfx::DrawableClass> FindDrawableClassById(const std::string& name) const override;
    virtual ClassHandle<const game::EntityClass> FindEntityClassByName(const std::string& name) const override;
    virtual ClassHandle<const game::EntityClass> FindEntityClassById(const std::string& id) const override;
    virtual ClassHandle<const game::SceneClass> FindSceneClassByName(const std::string& name) const override;
    virtual ClassHandle<const game::SceneClass> FindSceneClassById(const std::string& id) const override;
    // ContentLoader impl
    virtual bool LoadFromFile(const std::string& file) override;
private:
    std::string mResourceFile;
    // These are the material types that have been loaded
    // from the resource file.
    std::unordered_map<std::string,
            std::shared_ptr<gfx::MaterialClass>> mMaterials;
    // These are the particle engine types that have been loaded
    // from the resource file.
    std::unordered_map<std::string,
            std::shared_ptr<gfx::KinematicsParticleEngineClass>> mParticleEngines;
    // These are the custom shapes (polygons) that have been loaded
    // from the resource file.
    std::unordered_map<std::string,
            std::shared_ptr<gfx::PolygonClass>> mCustomShapes;
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
};

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

ClassHandle<const gfx::MaterialClass> ContentLoaderImpl::FindMaterialClassById(const std::string& name) const
{
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        if ("_" + color_name == name)
        {
            auto ret = std::make_shared<gfx::ColorClass>(gfx::CreateMaterialClassFromColor(gfx::Color4f(val)));
            ret->SetId("_" + color_name);
            return ret;
        }
    }

    auto it = mMaterials.find(name);
    if (it != std::end(mMaterials))
        return it->second;

    return nullptr;
}

ClassHandle<const gfx::DrawableClass> ContentLoaderImpl::FindDrawableClassById(const std::string& name) const
{
    // these are the current primitive cases that are not packed as part of the resources
    if (name == "_rect")
        return std::make_shared<gfx::RectangleClass>();
    else if (name == "_isosceles_triangle")
        return std::make_shared<gfx::IsoscelesTriangleClass>();
    else if (name == "_right_triangle")
        return std::make_shared<gfx::RightTriangleClass>();
    else if (name == "_capsule")
        return std::make_shared<gfx::CapsuleClass>();
    else if (name == "_circle")
        return std::make_shared<gfx::CircleClass>();
    else if (name == "_semi_circle")
        return std::make_shared<gfx::SemiCircleClass>();
    else if (name == "_round_rect")
        return std::make_shared<gfx::RoundRectangleClass>("_round_rect", 0.05f);
    else if (name == "_trapezoid")
        return std::make_shared<gfx::TrapezoidClass>();
    else if (name == "_parallelogram")
        return std::make_shared<gfx::ParallelogramClass>();
    else if (name == "_arrow_cursor")
        return std::make_shared<gfx::CursorClass>(gfx::CursorClass("_arrow_cursor",gfx::CursorClass::Shape::Arrow));
    else if (name == "_block_cursor")
        return std::make_shared<gfx::CursorClass>(gfx::CursorClass("_block_cursor", gfx::CursorClass::Shape::Block));

    // todo: perhaps need an identifier for the type of resource in question.
    // currently there's a name conflict that objects of different types but
    // with same names cannot be fully resolved by name only.

    {
        auto it = mParticleEngines.find(name);
        if (it != std::end(mParticleEngines))
            return it->second;
    }

    {
        auto it = mCustomShapes.find(name);
        if (it != std::end(mCustomShapes))
            return it->second;
    }

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
        std::optional<Implementation> ret = Implementation::FromJson(*chunk);
        if (!ret.has_value())
        {
            ERROR("Failed to load game class. [type='%1', name='%2]", type, name);
            return false;
        }

        out[id] = std::make_shared<Implementation>(std::move(ret.value()));
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
        auto ret = gfx::MaterialClass::FromJson(*chunk);
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

bool ContentLoaderImpl::LoadFromFile(const std::string& file)
{
    data::JsonFile json;
    const auto [success, error] = json.Load(file);
    if (!success)
    {
        ERROR("Failed to load game content from file. [file=%1', error='%2']", file, error);
        return false;
    }
    data::JsonObject root = json.GetRootObject();

    if (!LoadMaterials(root, "materials", mMaterials, nullptr))
        return false;
    if (!LoadContent<gfx::KinematicsParticleEngineClass>(root, "particles", mParticleEngines, nullptr))
        return false;
   if (!LoadContent<gfx::PolygonClass>(root, "shapes", mCustomShapes, nullptr))
       return false;
   if (!LoadContent<game::EntityClass>(root, "entities", mEntities, &mEntityNameTable))
       return false;
   if (!LoadContent<game::SceneClass>(root, "scenes", mScenes, &mSceneNameTable))
       return false;
   if (!LoadContent<uik::Window>(root, "uis", mWindows, nullptr))
       return false;
   if (!LoadContent<audio::GraphClass>(root, "audio_graphs", mAudioGraphs, nullptr))
        return false;

    // need to resolve the entity references.
    for (auto& p : mScenes)
    {
        auto& scene = p.second;
        for (size_t i=0; i<scene->GetNumNodes(); ++i)
        {
            auto& node = scene->GetNode(i);
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
    mResourceFile = file;
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

// static
std::unique_ptr<JsonFileClassLoader> JsonFileClassLoader::Create()
{ return std::make_unique<ContentLoaderImpl>(); }

} // namespace
