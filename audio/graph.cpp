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

#include <unordered_set>
#include <set>

#include "base/utility.h"
#include "base/format.h"
#include "base/logging.h"
#include "base/hash.h"
#include "data/writer.h"
#include "data/reader.h"
#include "audio/graph.h"
#include "audio/element.h"

namespace audio
{

GraphClass::GraphClass(const std::string& name, const std::string& id)
  : mName(name)
  , mId(id)
{}
GraphClass::GraphClass(const std::string& name)
  : GraphClass(name, base::RandomString(10))
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
    }
    return hash;
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
        writer.AppendChunk("elements", std::move(chunk));
    }
}

// static
std::optional<GraphClass> GraphClass::FromJson(const data::Reader& reader)
{
    std::string name;
    std::string id;
    if (!reader.Read("name", &name) ||
        !reader.Read("id",   &id))
        return std::nullopt;

    GraphClass ret(name, id);
    if (!reader.Read("src_elem_id",   &ret.mSrcElemId) ||
        !reader.Read("src_elem_port", &ret.mSrcElemPort))
        return std::nullopt;

    for (unsigned i=0; i<reader.GetNumChunks("links"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("links", i);
        Link link;
        chunk->Read("id",       &link.id);
        chunk->Read("src_elem", &link.src_element);
        chunk->Read("src_port", &link.src_port);
        chunk->Read("dst_elem", &link.dst_element);
        chunk->Read("dst_port", &link.dst_port);
        ret.mLinks.push_back(std::move(link));
    }
    for (unsigned i=0; i<reader.GetNumChunks("elements"); ++i)
    {
        const auto& chunk = reader.GetReadChunk("elements", i);
        Element elem;
        chunk->Read("id",   &elem.id);
        chunk->Read("name", &elem.name);
        chunk->Read("type", &elem.type);
        const auto* desc = FindElementDesc(elem.type);
        if (desc == nullptr)
            return std::nullopt;
        // copy over the list of arguments from the descriptor.
        // this conveniently gives us the argument names *and*
        // the expected types for reading the variant args back
        // from the JSON.
        elem.args = desc->args;
        for (auto& pair : elem.args)
        {
            const auto& name = "arg_" + pair.first;
            auto& variant = pair.second;
            std::visit([&chunk, &name](auto& variant_value) {
                chunk->Read(name.c_str(), &variant_value);
            }, variant);
        }
        ret.mElements.push_back(std::move(elem));
    }
    return ret;
}

Graph::Graph(const std::string& name, const std::string& id)
  : mName(name)
  , mId(id)
  , mPort("port")
{}
Graph::Graph(const std::string& name)
  : Graph(name, base::RandomString(10))
{}

Graph::Graph(const GraphClass& klass) :
  Graph(klass.GetName())
{
    for (size_t i=0; i< klass.GetNumElements(); ++i)
    {
        const auto& elem = klass.GetElement(i);
        ElementCreateArgs args;
        args.args = elem.args;
        args.id   = elem.id;
        args.type = elem.type;
        args.name = elem.name;
        mElements.push_back(CreateElement(args));
    }
    for (size_t i=0; i<klass.GetNumLinks(); ++i)
    {
        const auto& link = klass.GetLink(i);
        auto* src_elem = FindElementById(link.src_element);
        auto* dst_elem = FindElementById(link.dst_element);
        auto* src_port = src_elem->FindOutputPortByName(link.src_port);
        auto* dst_port = dst_elem->FindInputPortByName(link.dst_port);
        LinkElements(src_elem, src_port, dst_elem, dst_port);
    }
    auto* src_elem = FindElementById(klass.GetGraphOutputElementId());
    auto* src_port = src_elem->FindOutputPortByName(klass.GetGraphOutputElementPort());
    LinkGraph(src_elem, src_port);
}

Graph::Graph(std::shared_ptr<const GraphClass> klass)
  : Graph(*klass)
{}

Graph::Graph(Graph&& other)
  : mName(other.mName)
  , mId(other.mId)
  , mSrcMap(std::move(other.mSrcMap))
  , mDstMap(std::move(other.mDstMap))
  , mPortMap(std::move(other.mPortMap))
  , mElements(std::move(other.mElements))
  , mTopoOrder(std::move(other.mTopoOrder))
  , mFormat(std::move(other.mFormat))
  , mPort(std::move(other.mPort))
  , mDone(other.mDone)
{
    for (auto& pair : mPortMap)
    {
        auto* port = pair.second;
        if (port == &other.mPort)
        {
            pair.second = &mPort;
            break;
        }
    }
}

Element* Graph::AddElementPtr(std::unique_ptr<Element> element)
{
    mElements.push_back(std::move(element));
    return mElements.back().get();
}

Element* Graph::FindElementById(const std::string& id)
{
    return base::SafeFind(mElements, [id](const auto& item) {
        return item->GetId() == id;
    });
}
Element* Graph::FindElementByName(const std::string& name)
{
    return base::SafeFind(mElements, [name](const auto& item) {
        return item->GetName() == name;
    });
}

const Element* Graph::FindElementById(const std::string& id) const
{
    return base::SafeFind(mElements, [id](const auto& item) {
        return item->GetId() == id;
    });
}
const Element* Graph::FindElementByName(const std::string& name) const
{
    return base::SafeFind(mElements, [name](const auto& item) {
        return item->GetName() == name;
    });
}

void Graph::LinkElements(Element* src_elem, Port* src_port,
                         Element* dst_elem, Port* dst_port)
{
    ASSERT(HasElement(src_elem));
    ASSERT(HasElement(dst_elem));
    ASSERT(src_elem->HasOutputPort(src_port));
    ASSERT(dst_elem->HasInputPort(dst_port));
    mSrcMap[dst_elem].insert(src_elem);
    mDstMap[src_elem].insert(dst_elem);
    mPortMap[src_port] = dst_port;
}

void Graph::LinkGraph(Element* src_elem, Port* src_port)
{
    ASSERT(HasElement(src_elem));
    ASSERT(src_elem->HasOutputPort(src_port));
    mPortMap[src_port] = &mPort;
}

bool Graph::LinkElements(const std::string& src_elem_name, const std::string& src_port_name,
                         const std::string& dst_elem_name, const std::string& dst_port_name)
{
    auto* src_elem = FindElementByName(src_elem_name);
    auto* dst_elem = FindElementByName(dst_elem_name);
    if (src_elem == nullptr || dst_elem == nullptr)
        return false;
    auto* src_port = src_elem->FindOutputPortByName(src_port_name);
    auto* dst_port = dst_elem->FindInputPortByName(dst_port_name);
    if (src_port == nullptr || dst_port == nullptr)
        return false;
    LinkElements(src_elem, src_port, dst_elem, dst_port);
    return true;
}

bool Graph::LinkGraph(const std::string& src_elem_name, const std::string& src_port_name)
{
    auto* src_elem = FindElementByName(src_elem_name);
    if (src_elem == nullptr) return false;
    auto* src_port = src_elem->FindOutputPortByName(src_port_name);
    if (src_port == nullptr) return false;
    LinkGraph(src_elem, src_port);
    return true;
}

std::vector<std::string> Graph::Describe() const
{
    // find the set of all nodes with no incoming edges.
    std::unordered_set<Element*> set;
    for (const auto& e : mElements)
    {
        auto it = mSrcMap.find(e.get());
        if (it == mSrcMap.end())
            set.insert(e.get());
    }

    std::vector<std::string> ret;
    // trace every path starting from every root node
    while (!set.empty())
    {
        Element* src = *set.begin();
        base::AppendVector(ret, DescribePaths(src));
        set.erase(set.begin());
    }
    return ret;
}

std::vector<std::string> Graph::DescribePaths(const Element* src) const
{
    std::vector<std::string> ret;
    for (unsigned i=0; i<src->GetNumOutputPorts(); ++i)
    {
        auto& src_port = const_cast<Element*>(src)->GetOutputPort(i);
        auto* dst_port = FindDstPort(&src_port);
        auto* dst_elem = FindInputPortOwner(dst_port);
        if (dst_elem == nullptr && dst_port == &mPort)
            dst_elem = this;
        if (dst_elem == nullptr)
        {
            ret.push_back(base::FormatString("%1:%2 -> nil", src->GetName(), src_port.GetName()));
            continue;
        }
        auto paths = DescribePaths(dst_elem);
        for (auto path : paths)
        {
            ret.push_back(base::FormatString("%1:%2 -> %3:%4 %5", src->GetName(), src_port.GetName(),
                                             dst_elem->GetName(), dst_port->GetName(), path));
        }
    }
    return ret;
}

Port* Graph::FindDstPort(const Port* src)
{
    auto it = mPortMap.find(const_cast<Port*>(src));
    if (it == mPortMap.end())
        return nullptr;
    return it->second;
}

const Port* Graph::FindDstPort(const Port* src) const
{
    auto it = mPortMap.find(const_cast<Port*>(src));
    if (it == mPortMap.end())
        return nullptr;
    return it->second;
}

Port* Graph::FindSrcPort(const Port* dst)
{
    for (auto pair : mPortMap)
    {
        if (pair.second == dst)
            return pair.first;
    }
    return nullptr;
}

const Port* Graph::FindSrcPort(const Port* dst) const
{
    for (auto pair : mPortMap)
    {
        if (pair.second == dst)
            return pair.first;
    }
    return nullptr;
}

Element* Graph::FindInputPortOwner(const Port* port)
{
    for (auto& elem : mElements)
    {
        for (unsigned i=0; i<elem->GetNumInputPorts(); ++i)
        {
            auto& p = elem->GetInputPort(i);
            if (&p == port)
                return elem.get();
        }
    }
    return nullptr;
}

Element* Graph::FindOutputPortOwner(const Port* port)
{
    for (auto& elem : mElements)
    {
        for (unsigned i=0; i<elem->GetNumOutputPorts(); ++i)
        {
            auto& p = elem->GetOutputPort(i);
            if (&p == port)
                return elem.get();
        }
    }
    return nullptr;
}

const Element* Graph::FindInputPortOwner(const Port* port) const
{
    for (auto& elem : mElements)
    {
        for (unsigned i=0; i<elem->GetNumInputPorts(); ++i)
        {
            auto& p = elem->GetInputPort(i);
            if (&p == port)
                return elem.get();
        }
    }
    return nullptr;
}

const Element* Graph::FindOutputPortOwner(const Port* port) const
{
    for (auto& elem : mElements)
    {
        for (unsigned i=0; i<elem->GetNumOutputPorts(); ++i)
        {
            auto& p = elem->GetOutputPort(i);
            if (&p == port)
                return elem.get();
        }
    }
    return nullptr;
}

bool Graph::IsSrcPortTaken(const Port* src) const
{
    auto it = mPortMap.find(const_cast<Port*>(src));
    if (it == mPortMap.end())
        return false;
    return true;
}
bool Graph::IsDstPortTaken(const Port* dst) const
{
    for (auto pair : mPortMap)
    {
        if (pair.second == dst)
            return true;
    }
    return false;
}

bool Graph::HasElement(Element* element) const
{
    for (const auto& e : mElements)
        if (e.get() == element) return true;
    return false;
}

bool Graph::Prepare(const Loader& loader)
{
    // This is the so called Kahn's algorithm
    // https://en.wikipedia.org/wiki/Topological_sorting
    auto edges = mSrcMap;

    // find set of all nodes with no incoming edges.
    std::unordered_set<Element*> set;
    for (auto& e : mElements)
    {
        auto it = edges.find(e.get());
        if (it == edges.end())
            set.insert(e.get());
    }

    // this is the topological order of traversal
    // of the elements.
    std::vector<Element*> order;

    while (!set.empty())
    {
        Element* src = *set.begin();
        set.erase(set.begin());
        order.push_back(src);

        for (auto it = edges.begin(); it != edges.end();)
        {
            auto* dst_elem  = it->first;
            auto& src_edges = it->second;
            src_edges.erase(src);
            if (src_edges.empty())
            {
                set.insert(dst_elem);
                it = edges.erase(it);
            } else ++it;
        }
    }
    if (!edges.empty()) ERROR_RETURN(false, "Audio graph '%1' has a cycle.", mName);

    for (size_t i=0; i < order.size(); ++i)
    {
        Element* element = order[i];
        DEBUG("Audio graph '%1' preparing audio element '%2'", mName, element->GetName());
        if (!element->Prepare(loader))
        {
            ERROR("Audio graph '%1' element '%2' failed to prepare.", mName, element->GetName());
            return false;
        }
        for (unsigned i=0; i < element->GetNumOutputPorts(); ++i)
        {
            auto& src_port = element->GetOutputPort(i);
            // does this port have a destination port ?
            if (auto* dst_port = FindDstPort(&src_port))
            {
                if (!dst_port->CanAccept(src_port.GetFormat()))
                {
                    const auto* dst_elem = FindInputPortOwner(dst_port);
                    ERROR("Ports %1:%2 and %3:%4 are not compatible.",
                          element->GetName(), src_port.GetName(),
                          dst_elem->GetName(), dst_port->GetName());
                    return false;
                }
                dst_port->SetFormat(src_port.GetFormat());
            }
            else
            {
                WARN("Audio graph '%1' source %2:%3 has no input port assigned.",
                     mName, element->GetName(), src_port.GetName());
            }
        }
        for (unsigned i=0; i < element->GetNumInputPorts(); ++i)
        {
            auto& dst_port = element->GetInputPort(i);
            auto* src_port = FindSrcPort(&dst_port);
            if (src_port == nullptr)
            {
                WARN("Audio graph '%1' input %2:%3 has no source port assigned.",
                     mName, element->GetName(), dst_port.GetName());
            }
        }
    }
    mTopoOrder = std::move(order);
    mFormat    = mPort.GetFormat();
    if (!IsValid(mFormat))
    {
        ERROR("Audio graph '%1' output format (%2) is not valid.", mName, mFormat);
        return false;
    }
    INFO("Audio graph '%1' output set to '%2'.", mName, mFormat);
    return true;
}

void Graph::Process(EventQueue& events, unsigned milliseconds)
{
    // Evaluate the elements in topological order and then
    // dispatch the buffers according to the element/port links.
    for (auto& source : mTopoOrder)
    {
        // this element could be done but the pipeline could still
        // have pending buffers in the port queues.
        if (source->IsSource() && source->IsSourceDone())
            continue;

        bool backpressure = false;
        // find out if the next element is putting back pressure
        // on the source element by not consuming the input buffers.
        // if this is the case the producer evaluation is skipped
        // and the graph will subsequently likely stop producing
        // output buffers.
        for (unsigned i=0; i<source->GetNumOutputPorts(); ++i)
        {
            auto& src = source->GetOutputPort(i);
            auto* dst = FindDstPort(&src);
            if (dst && dst->IsFull())
            {
                backpressure = true;
                break;
            }
        }
        if (backpressure)
            continue;

        // process the audio buffers.
        source->Process(events, milliseconds);

        // dispatch the resulting buffers by iterating over the output
        // ports and finding their assigned input ports.
        for (unsigned i=0; i<source->GetNumOutputPorts(); ++i)
        {
            auto& output = source->GetOutputPort(i);
            BufferHandle buffer;
            if (!output.PullBuffer(buffer))
                continue;

            if (auto* dst = FindDstPort(&output))
                dst->PushBuffer(buffer);
        }
    }

    // check if we're all done
    bool graph_done = true;
    for (const auto& element : mTopoOrder)
    {
        if (element->IsSource() && !element->IsSourceDone())
            graph_done = false;

        if (!graph_done)
            break;

        for (unsigned i=0; i<element->GetNumOutputPorts(); ++i)
        {
            auto& port = element->GetOutputPort(i);
            if (!port.HasBuffers())
                continue;
            graph_done = false;
            break;
        }
        if (!graph_done)
            break;
    }

    if (graph_done)
    {
        DEBUG("Audio graph '%1 ' is done!", mName);
        for (auto& element : mTopoOrder)
        {
            DEBUG("Shutting down audio graph '%1' element '%2'", mName, element->GetName());
            element->Shutdown();
        }
    }
    mDone = graph_done;
}

void Graph::Shutdown()
{

}

void Graph::Advance(unsigned int ms)
{
    for (auto& elem : mElements)
    {
        elem->Advance(ms);
    }
}

bool Graph::DispatchCommand(const std::string& dest, Element::Command& cmd)
{
    // see if the receiver of the command is a direct descendant
    for (auto& elem : mElements)
    {
        if (elem->GetName() != dest)
            continue;
        elem->ReceiveCommand(cmd);
        return true;
    }
    // try to dispatch the command recursively.
    for (auto& elem : mElements)
    {
        if (elem->DispatchCommand(dest, cmd))
            return true;
    }
    return false;
}

struct AudioGraph::GraphCmd {
    std::unique_ptr<Element::Command> cmd;
    std::string dest;
};

AudioGraph::AudioGraph(const std::string& name)
  : mName(name)
  , mGraph(name)
{}

AudioGraph::AudioGraph(const std::string& name, Graph&& graph)
  : mName(name)
  , mGraph(std::move(graph))
{}

AudioGraph::AudioGraph(AudioGraph&& other)
  : mName(other.mName)
  , mGraph(std::move(other.mGraph))
{}

bool AudioGraph::Prepare(const Loader& loader)
{
    if (!mGraph.Prepare(loader))
        return false;
    auto& out = mGraph.GetOutputPort(0);
    mFormat = out.GetFormat();
    return true;
}

unsigned AudioGraph::GetRateHz() const noexcept
{ return mFormat.sample_rate; }

unsigned AudioGraph::GetNumChannels() const noexcept
{ return mFormat.channel_count; }

Source::Format AudioGraph::GetFormat() const noexcept
{
    if (mFormat.sample_type == SampleType::Int16)
        return Source::Format::Int16;
    else if (mFormat.sample_type == SampleType::Int32)
        return Source::Format::Int32;
    else if (mFormat.sample_type == SampleType::Float32)
        return Source::Format::Float32;
    else BUG("Incorrect audio format.");
    return Source::Format::Float32;
}
std::string AudioGraph::GetName() const noexcept
{ return mName; }

unsigned AudioGraph::FillBuffer(void* buff, unsigned max_bytes)
{
    // todo: figure out a way to get rid of this copying whenever possible.

    // compute how many milliseconds worth of data can we
    // stuff into the current buffer. todo: should the buffering
    // sizes be fixed somewhere somehow? I.e. we're always dispatching
    // let's say 5ms worth of audio or whatever ?
    const auto millis_in_bytes = GetMillisecondByteCount(mFormat);
    const auto milliseconds = max_bytes / millis_in_bytes;
    const auto bytes = millis_in_bytes * milliseconds;
    ASSERT(bytes <= max_bytes);

    if (mPendingBuffer)
    {
        const auto pending = mPendingBuffer->GetByteSize() - mPendingOffset;
        const auto min_bytes = std::min(pending, (size_t)max_bytes);
        const auto* ptr = static_cast<const uint8_t*>(mPendingBuffer->GetPtr());
        std::memcpy(buff, ptr + mPendingOffset, min_bytes);
        mPendingOffset += min_bytes;
        if (mPendingOffset == mPendingBuffer->GetByteSize())
        {
            mPendingBuffer.reset();
            mPendingOffset = 0;
        }
        return min_bytes;
    }

    mGraph.Process(mEvents, milliseconds);
    mGraph.Advance(milliseconds);
    mMillisecs += milliseconds;

    BufferHandle buffer;
    auto& port = mGraph.GetOutputPort(0);
    if (port.PullBuffer(buffer))
    {
        const auto min_bytes = std::min(buffer->GetByteSize(), (size_t)max_bytes);
        std::memcpy(buff, buffer->GetPtr(), min_bytes);
        if (min_bytes < buffer->GetByteSize())
        {
            ASSERT(!mPendingBuffer && !mPendingOffset);
            mPendingBuffer = buffer;
            mPendingOffset = min_bytes;
        }
        return min_bytes;
    }
    else if (!mGraph.IsSourceDone())
    {
        // currently if the audio graph isn't producing any data the pulseaudio
        // playback stream will automatically go into paused state. (done by pulseaudio)
        // right now we don't have a mechanism to signal the resumption of the
        // playback i.e. resume the PA stream in order to have another stream
        // write callback on time when the graph is producing data again.
        // So in case there's no graph output but the graph is not yet finished
        // we just fill the buffer with 0s and return that.
        std::memset(buff, 0, max_bytes);
        return max_bytes;
    }

    WARN("Audio graph '%1' has no output audio buffer available.", mName);
    return 0;
}
bool AudioGraph::HasMore(std::uint64_t num_bytes_read) const noexcept
{
    return mPendingBuffer || !mGraph.IsSourceDone();
}
bool AudioGraph::Reset() noexcept
{
    // todo:
    BUG("Unimplemented function.");
    return false;
}
void AudioGraph::RecvCommand(std::unique_ptr<Command> cmd) noexcept
{
    if (auto* ptr = cmd->GetIf<GraphCmd>())
    {
        if (!mGraph.DispatchCommand(ptr->dest, *ptr->cmd))
            WARN("Audio graph '%1' command receiver element '%2' not found.", mName, ptr->dest);
    }
    else BUG("Unexpected command.");
}

std::unique_ptr<Event> AudioGraph::GetEvent() noexcept
{
    if (mEvents.empty()) return nullptr;
    auto ret = std::move(mEvents.front());
    mEvents.pop();
    return ret;
}

// static
std::unique_ptr<Command> AudioGraph::MakeCommandPtr(const std::string& destination, std::unique_ptr<Element::Command>&& cmd)
{
    GraphCmd foo;
    foo.dest = destination;
    foo.cmd  = std::move(cmd);
    return audio::MakeCommand(std::move(foo));
}

} // namespace
