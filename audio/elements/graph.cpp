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

#include "base/assert.h"
#include "base/logging.h"
#include "base/trace.h"
#include "audio/element.h"
#include "audio/elements/graph.h"
#include "audio/elements/graph_class.h"
#include "audio/elements/queue.h"

namespace audio
{

Graph::Graph(const std::string& name, const std::string& id)
  : mName(name)
  , mId(id)
  , mPort("port")
{}
Graph::Graph(const std::string& name)
  : Graph(name, base::RandomString(10))
{}

Graph::Graph(const std::string& name, const GraphClass& klass) :
  Graph(name)
{
    for (size_t i=0; i< klass.GetNumElements(); ++i)
    {
        const auto& elem = klass.GetElement(i);
        ElementCreateArgs args;
        args.args = elem.args;
        args.id   = elem.id;
        args.type = elem.type;
        args.name = elem.name;
        args.input_ports  = elem.input_ports;
        args.output_ports = elem.output_ports;
        mElements.push_back(CreateElement(args));
    }
    for (size_t i=0; i<klass.GetNumLinks(); ++i)
    {
        const auto& link = klass.GetLink(i);
        auto* src_elem = FindElementById(link.src_element);
        auto* dst_elem = FindElementById(link.dst_element);
        if (!src_elem || !dst_elem)
            continue;
        auto* src_port = src_elem->FindOutputPortByName(link.src_port);
        auto* dst_port = dst_elem->FindInputPortByName(link.dst_port);
        if (!src_port || !dst_port)
            continue;
        LinkElements(src_elem, src_port, dst_elem, dst_port);
    }
    auto* src_elem = FindElementById(klass.GetGraphOutputElementId());
    if (src_elem == nullptr)
        return;
    auto* src_port = src_elem->FindOutputPortByName(klass.GetGraphOutputElementPort());
    if (src_port == nullptr)
        return;
    LinkGraph(src_elem, src_port);
}
Graph::Graph(const GraphClass& klass)
  : Graph(klass.GetName(), klass)
{}

Graph::Graph(std::shared_ptr<const GraphClass> klass)
  : Graph(*klass)
{}
Graph::Graph(const std::string& name, std::shared_ptr<const GraphClass> klass)
  : Graph(name, *klass)
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

bool Graph::Prepare(const Loader& loader, const PrepareParams& params)
{
    bool graph_output_port_set = false;
    for (const auto& [src_port, dst_port] : mPortMap)
    {
        if (dst_port == &mPort)
        {
            graph_output_port_set = true;
            break;
        }
    }
    if (!graph_output_port_set)
    {
        ERROR("Failed to determine audio graph output port. No output element selected. [graph='%1']", mName);
        return false;
    }

    TRACE_SCOPE("Graph::Prepare");

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
    if (!edges.empty())
        ERROR_RETURN(false, "Audio graph cycle detected. [graph=%1]", mName);

    DEBUG("Preparing audio graph. [graph=%1]", mName);

    for (size_t i=0; i < order.size(); ++i)
    {
        Element* element = order[i];
        //DEBUG("Prepare audio graph element. [graph=%1, elem=%2]", mName, element->GetName());
        if (!element->Prepare(loader, params))
        {
            ERROR("Audio graph element failed to prepare.[graph=%1, elem=%2]", mName, element->GetName());
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
                    ERROR("Audio graph element link between incompatible ports. [src=%1:%2, dst=%3:%4]",
                          element->GetName(), src_port.GetName(),
                          dst_elem->GetName(), dst_port->GetName());
                    return false;
                }
                dst_port->SetFormat(src_port.GetFormat());
            }
            else
            {
                WARN("Audio graph element output port has no destination port assigned. [graph=%1, elem=%2, port=%3]",
                     mName, element->GetName(), src_port.GetName());
            }
        }
        for (unsigned i=0; i < element->GetNumInputPorts(); ++i)
        {
            auto& dst_port = element->GetInputPort(i);
            auto* src_port = FindSrcPort(&dst_port);
            if (src_port == nullptr)
            {
                WARN("Audio graph element input port has no source port assigned. [graph=%1 elem=%2, port=%3]",
                     mName, element->GetName(), dst_port.GetName());
            }
        }
    }
    mTopoOrder = std::move(order);
    mFormat    = mPort.GetFormat();
    if (!IsValid(mFormat))
    {
        ERROR("Audio graph output format is not valid. [graph=%1, format=%2]", mName, mFormat);
        return false;
    }
    DEBUG("Audio graph prepared successfully. [graph=%1, output=%2]", mName, mFormat);
    return true;
}

void Graph::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Graph");

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
        {
            if (source->GetType() != "Queue")
                continue;
        }

        // process the audio buffers.
        TRACE_CALL("Element::Process", source->Process(allocator, events, milliseconds));

        // dispatch the resulting buffers by iterating over the output
        // ports and finding their assigned input ports.
        for (unsigned i=0; i<source->GetNumOutputPorts(); ++i)
        {
            auto& output = source->GetOutputPort(i);
            BufferHandle buffer;
            if (!output.PullBuffer(buffer))
                continue;

            Buffer::InfoTag tag;
            tag.element.name   = source->GetName();
            tag.element.id     = source->GetId();
            tag.element.source = source->IsSource();
            tag.element.source_done = source->IsSourceDone();
            buffer->AddInfoTag(tag);

            if (auto* dst = FindDstPort(&output))
            {
                if (!dst->PushBuffer(buffer))
                    output.PushBuffer(buffer);
            }
        }
    }

    // check if we're all done
    bool graph_done = true;
    for (const auto& element : mTopoOrder)
    {
        if (element->IsSource() && !element->IsSourceDone())
            graph_done = false;

        if (element->GetType() == "Queue")
        {
            const auto* queue = static_cast<const Queue*>(element);

            if (!queue->IsEmpty())
                graph_done = false;
        }

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
        DEBUG("Audio graph is done. [graph=%1]", mName);
    }
    mDone = graph_done;
}

void Graph::Shutdown()
{
    for (auto& element : mTopoOrder)
    {
        DEBUG("Shutting down audio graph element. [graph=%1, elem=%2]", mName, element->GetName());
        element->Shutdown();
    }
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



} // namespace