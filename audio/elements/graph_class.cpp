// Copyright (C) 2020-2025 Sami Väisänen
// Copyright (C) 2020-2025 Ensisoft http://www.ensisoft.com
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

#include <set>

#include "base/assert.h"
#include "base/logging.h"
#include "base/trace.h"
#include "base/utility.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "audio/elements/graph_class.h"

namespace {
    template<typename T>
    const T* GetOptionalArg(const std::unordered_map<std::string, audio::ElementArg>& args,
                            const std::string& arg_name,
                            const std::string& elem)
    {
        if (const auto* variant = base::SafeFind(args, arg_name))
        {
            if (const auto* value = std::get_if<T>(variant))
                return value;
            else WARN("Mismatch in audio element argument type. [elem=%1, arg=%2]", elem, arg_name);
        }
        return nullptr;
    }
}


namespace audio
{

GraphClass::GraphClass(std::string name, std::string id)
  : mName(std::move(name))
  , mId(std::move(id))
{}

GraphClass::GraphClass(std::string name)
  : GraphClass(std::move(name), base::RandomString(10))
{}

std::size_t GraphClass::GetHash() const
{
    size_t hash = 0;
    hash = base::hash_combine(hash, mName);
    hash = base::hash_combine(hash, mId);
    hash = base::hash_combine(hash, mSrcElemId);
    hash = base::hash_combine(hash, mSrcElemPort);
    for (const auto& link : mLinks)
    {
        hash = base::hash_combine(hash, link.id);
        hash = base::hash_combine(hash, link.src_port);
        hash = base::hash_combine(hash, link.src_element);
        hash = base::hash_combine(hash, link.dst_port);
        hash = base::hash_combine(hash, link.dst_element);
    }
    for (const auto& elem : mElements)
    {
        hash = base::hash_combine(hash, elem.id);
        hash = base::hash_combine(hash, elem.name);
        hash = base::hash_combine(hash, elem.type);

        std::set<std::string> keys;
        for (const auto& arg : elem.args)
            keys.insert(arg.first);

        for (const auto& key : keys)
        {
            auto it = elem.args.find(key);
            ASSERT(it != elem.args.end());
            hash = base::hash_combine(hash, it->second);
        }
        for (const auto& port : elem.input_ports)
        {
            hash = base::hash_combine(hash, port.name);
        }
        for (const auto& port : elem.output_ports)
        {
            hash = base::hash_combine(hash, port.name);
        }
    }
    return hash;
}

void GraphClass::Preload(const Loader& loader, const PreloadParams& params) const
{
    for (const auto& element : mElements)
    {
        if (element.type != "FileSource")
            continue;

        auto source = CreateElement(element);
        auto* file = static_cast<FileSource*>(source.get());

        const auto* file_caching = GetOptionalArg<bool>(element.args, "file_caching", element.name);
        const auto* pcm_caching = GetOptionalArg<bool>(element.args, "pcm_caching", element.name);

        DEBUG("Probing audio file '%1'", file->GetFileName());

        audio::Element::PrepareParams prep;
        prep.enable_pcm_caching = params.enable_pcm_caching;
        if (!source->Prepare(loader, prep))
            continue;

        // hack hack hack but set the loop count to 1 to make
        // sure that we're not stuck indefinitely in the loop
        // below trying to complete the source.
        file->SetLoopCount(1);

        if (pcm_caching && *pcm_caching)
        {
            DEBUG("Decoding audio file '%1'", file->GetFileName());

            BufferAllocator allocator;
            BufferHandle buffer;
            std::queue<std::unique_ptr<Event>> events;
            do
            {
                source->Process(allocator, events, 20);
                auto& out = source->GetOutputPort(0);
                out.PullBuffer(buffer);
            }
            while (!source->IsSourceDone());
        }
    }
}

void GraphClass::IntoJson(data::Writer& writer) const
{
    writer.Write("name", mName);
    writer.Write("id",   mId);
    writer.Write("src_elem_id", mSrcElemId);
    writer.Write("src_elem_port", mSrcElemPort);
    for (const auto& link : mLinks)
    {
        auto chunk = writer.NewWriteChunk();
        chunk->Write("id",       link.id);
        chunk->Write("src_elem", link.src_element);
        chunk->Write("src_port", link.src_port);
        chunk->Write("dst_elem", link.dst_element);
        chunk->Write("dst_port", link.dst_port);
        writer.AppendChunk("links", std::move(chunk));
    }
    for (const auto& elem : mElements)
    {
        auto chunk = writer.NewWriteChunk();
        chunk->Write("id",   elem.id);
        chunk->Write("name", elem.name);
        chunk->Write("type", elem.type);
        for (const auto& pair : elem.args)
        {
            const auto& name    = "arg_" + pair.first;
            const auto& variant = pair.second;
            std::visit([&chunk, &name](const auto& variant_value) {
                chunk->Write(name.c_str(), variant_value);
            }, variant);
        }
        for (const auto& port : elem.output_ports)
        {
            auto port_chunk = writer.NewWriteChunk();
            port_chunk->Write("name", port.name);
            chunk->AppendChunk("output_ports", std::move(port_chunk));
        }
        for (const auto& port : elem.input_ports)
        {
            auto port_chunk = writer.NewWriteChunk();
            port_chunk->Write("name", port.name);
            chunk->AppendChunk("input_ports", std::move(port_chunk));
        }
        writer.AppendChunk("elements", std::move(chunk));
    }
}

bool GraphClass::FromJson(const data::Reader& reader)
{
    bool ok = true;
    ok &= reader.Read("name",          &mName);
    ok &= reader.Read("id",            &mId);
    ok &= reader.Read("src_elem_id",   &mSrcElemId);
    ok &= reader.Read("src_elem_port", &mSrcElemPort);

    for (unsigned i=0; i<reader.GetNumChunks("links"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("links", i);
        Link link;
        ok &= chunk->Read("id",       &link.id);
        ok &= chunk->Read("src_elem", &link.src_element);
        ok &= chunk->Read("src_port", &link.src_port);
        ok &= chunk->Read("dst_elem", &link.dst_element);
        ok &= chunk->Read("dst_port", &link.dst_port);
        mLinks.push_back(std::move(link));
    }
    for (unsigned i=0; i<reader.GetNumChunks("elements"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("elements", i);
        Element elem;
        ok &= chunk->Read("id",   &elem.id);
        ok &= chunk->Read("name", &elem.name);
        ok &= chunk->Read("type", &elem.type);
        const auto* desc = FindElementDesc(elem.type);
        if (desc == nullptr)
            continue;
        // copy over the list of arguments from the descriptor.
        // this conveniently gives us the argument names *and*
        // the expected types for reading the variant args back
        // from the JSON.
        elem.args = desc->args;
        for (auto& pair : elem.args)
        {
            const auto& name = "arg_" + pair.first;
            auto& variant = pair.second;
            std::visit([&chunk, &name, &ok](auto& variant_value) {
                ok &= chunk->Read(name.c_str(), &variant_value);
            }, variant);
        }
        for (unsigned i=0; i<chunk->GetNumChunks("output_ports"); ++i)
        {
            const auto& port_chunk = chunk->GetReadChunk("output_ports", i);
            PortDesc port;
            ok &= port_chunk->Read("name", &port.name);
            elem.output_ports.push_back(std::move(port));
        }
        for (unsigned i=0; i<chunk->GetNumChunks("input_ports"); ++i)
        {
            const auto& port_chunk = chunk->GetReadChunk("input_ports", i);
            PortDesc port;
            ok &= port_chunk->Read("name", &port.name);
            elem.input_ports.push_back(std::move(port));
        }
        mElements.push_back(std::move(elem));
    }
    return ok;
}

} // namespace