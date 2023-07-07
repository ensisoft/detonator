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

#include <vector>
#include <unordered_map>
#include <fstream>

#include "base/logging.h"
#include "base/utility.h"
#include "graphics/loader.h"

namespace {
static gfx::Loader* gLoader;

class FileResource : public gfx::Resource
{
public:
    FileResource(std::string filename)
    {
        auto in = base::OpenBinaryInputStream(filename);
        if (!in.is_open())
        {
            ERROR("Failed to open '%1' file.", filename);
            return;
        }
        std::vector<char> data;

        in.seekg(0, std::ios::end);
        const auto size = (std::size_t)in.tellg();
        in.seekg(0, std::ios::beg);
        data.resize(size);
        in.read(&data[0], size);
        if ((std::size_t)in.gcount() != size)
        {
            ERROR("Failed to read all of '%1'", filename);
            return;
        }
        mBuffer = std::move(data);
        mFilename = std::move(filename);
        mError = false;
        DEBUG("Loaded file: '%1'", mFilename);
    }
    virtual const void* GetData() const override
    {
        if (mBuffer.empty()) return nullptr;
        return &mBuffer[0];
    }
    virtual std::size_t GetByteSize() const override
    { return mBuffer.size(); }
    virtual std::string GetSourceName() const override
    { return mFilename; }

    bool IsGood() const
    { return !mError; }
private:
    std::string mFilename;
    std::vector<char> mBuffer;
    bool mError = true;
};

// this implementation currently exists as quick "works out of the box"
// type of solution. it's conceivable that more sophisticated file load
// methods could be done depending on how the resource packing is done.
// For example the resource packing process could package resources into
// a single blob that is then mapped into process's address space and
// resource loads are simply transformed into pointer offsets in the
// memory mapped address. Alternatively the actual IO could be done
// ahead of time by another thread for example.
std::shared_ptr<const FileResource> DefaultLoadFile(const std::string& file)
{
    using FileCache  = std::unordered_map<std::string, std::shared_ptr<const FileResource>>;
    // behind a flag since caching the files can actually break unit tests.
    // also it can also break Editor's "reload" functionality for reloading
    // shaders, textures etc. would need a way to purge the cache on such reload
    // so it's simply not enabled for now.
#if defined(GFX_ENABLE_DEFAULT_FILE_LOAD_CACHE)
    static FileCache cache;
    auto it = cache.find(file);
    if (it != cache.end())
        return it->second;
#endif

    auto buffer = std::make_shared<FileResource>(file);
    if (!buffer->IsGood())
        return nullptr;

#if defined(GFX_ENABLE_DEFAULT_FILE_LOAD_CACHE)
    cache[file] = buffer;
#endif
    return buffer;
}

} // namespace

namespace gfx
{

void SetResourceLoader(Loader* loader)
{ gLoader = loader; }

Loader* GetResourceLoader()
{ return gLoader; }

std::shared_ptr<const Resource> LoadResource(const std::string& URI)
{
    if (gLoader)
        return gLoader->LoadResource(URI);
    // if there's no resolver and the URI is actually an URI
    // the load will fail. if the application is using URIs
    // (instead of just file system paths directly) it should
    // set the loader.
    return DefaultLoadFile(URI);
}

} // namespace
