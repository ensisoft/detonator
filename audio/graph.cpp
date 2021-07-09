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

#include "base/utility.h"
#include "base/format.h"
#include "base/logging.h"
#include "audio/graph.h"
#include "audio/element.h"

namespace audio
{

Graph::Graph(const std::string& name)
  : mName(name)
  , mPort("out")
{}

Graph::~Graph()
{}

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
    mSrcMap[dst_elem].insert(src_elem);
    mDstMap[src_elem].insert(dst_elem);
    mPortMap[src_port] = dst_port;
}

void Graph::LinkGraph(Element* src_elem, Port* src_port)
{
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

bool Graph::Prepare()
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
    ASSERT(edges.empty() && "Cycle in audio graph.");

    for (size_t i=0; i < order.size(); ++i)
    {
        Element* src = order[i];
        DEBUG("Preparing audio element '%1'", src->GetName());
        if (!src->Prepare())
        {
            ERROR("Element '%1' failed to prepare.", src->GetName());
            return false;
        }
        for (unsigned i=0; i<src->GetNumOutputPorts(); ++i)
        {
            auto& src_port = src->GetOutputPort(i);
            // does this port have a destination port ?
            auto it = mPortMap.find(&src_port);
            if (it == mPortMap.end())
                continue;
            auto* dst_port = it->second;
            if (!dst_port->CanAccept(src_port.GetFormat()))
            {
                const auto* dst_elem = FindInputPortOwner(dst_port);
                ERROR("Ports %1:%2 and %3:%4 are not compatible.", src->GetName(), src_port.GetName(),
                      dst_elem->GetName(), dst_port->GetName());
                return false;
            }
            dst_port->SetFormat(src_port.GetFormat());
        }
    }
    mTopoOrder = std::move(order);
    mFormat    = mPort.GetFormat();
    DEBUG("Graph output set to %1 with %2 channels @ %3 Hz", mFormat.sample_type,
          mFormat.channel_count, mFormat.sample_rate);
    return true;
}

unsigned Graph::GetRateHz() const noexcept
{ return mFormat.sample_rate; }

unsigned Graph::GetNumChannels() const noexcept
{ return mFormat.channel_count; }

Source::Format Graph::GetFormat() const noexcept
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
std::string Graph::GetName() const noexcept
{ return mName; }

unsigned Graph::FillBuffer(void* buff, unsigned max_bytes)
{
    // compute how many milliseconds worth of data can we
    // stuff into the current buffer. todo: should the buffering
    // sizes be fixed somewhere somehow? I.e. we're always dispatching
    // let's say 5ms worth of audio or whatever ?
    const auto millis_in_bytes = GetMillisecondByteCount(mFormat);
    const auto milliseconds = max_bytes / millis_in_bytes;
    const auto bytes = millis_in_bytes * milliseconds;
    ASSERT(bytes <= max_bytes);

    // Evaluate the elements in topological order and then
    // dispatch the buffers according to the element/port
    // links. Fill the audio device buffer from the last
    // element of the ordering.
    for (auto& source : mTopoOrder)
    {
        // this element could be done but the pipeline could still
        // have pending buffers in the port queues.
        if (source->IsDone())
            continue;

        // process the audio buffers.
        source->Process(milliseconds);

        // dispatch the resulting buffers by iterating over the output
        // ports and finding their assigned input ports.
        for (unsigned i=0; i<source->GetNumOutputPorts(); ++i)
        {
            auto& output = source->GetOutputPort(i);
            BufferHandle buffer;
            if (!output.PullBuffer(buffer))
            {
                WARN("No audio buffer available from %1:%2", source->GetName(), output.GetName());
                continue;
            }
            auto it = mPortMap.find(&output);
            if (it == mPortMap.end())
            {
                WARN("Audio source %1:%2 has no input port assigned.", source->GetName(), output.GetName());
                continue;
            }

            auto* input = it->second;
            input->PushBuffer(buffer);
        }
    }

    // check if we're all done
    bool graph_done = true;
    for (const auto& element : mTopoOrder)
    {
        graph_done = graph_done && element->IsDone();
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
        DEBUG("Graph is done!");
        for (auto& element : mTopoOrder)
        {
            DEBUG("Shutting down audio element '%1'", element->GetName());
            element->Shutdown();
        }
    }
    mDone = graph_done;

    if (!mPort.HasBuffers())
    {
        WARN("No audio buffer available.");
        return 0;
    }

    BufferHandle buffer;
    mPort.PullBuffer(buffer);

    // todo: get rid of the copying.
    ASSERT(buffer->GetByteSize() <= max_bytes);
    std::memcpy(buff, buffer->GetPtr(), buffer->GetByteSize());
    return buffer->GetByteSize();
}
bool Graph::HasNextBuffer(std::uint64_t num_bytes_read) const noexcept
{
    return !mDone;
}
bool Graph::Reset() noexcept
{
    // todo:
    return true;
}

} // namespace
