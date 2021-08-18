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

#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "base/utility.h"
#include "data/fwd.h"
#include "audio/source.h"
#include "audio/format.h"
#include "audio/element.h"
#include "audio/command.h"

namespace audio
{
    class GraphClass
    {
    public:
        using ElementArg = audio::ElementArg;
        using Element    = audio::ElementCreateArgs;

        struct Link {
            std::string id;
            std::string src_element;
            std::string src_port;
            std::string dst_element;
            std::string dst_port;
        };
        GraphClass(const std::string& name, const std::string& id);
        GraphClass(const std::string& name);

        void AddElement(const Element& element)
        { mElements.push_back(element); }
        void AddElement(Element&& element)
        { mElements.push_back(std::move(element)); }
        void AddLink(const Link& edge)
        { mLinks.push_back(edge); }
        void AddLink(Link&& edge)
        { mLinks.push_back(std::move(edge)); }
        void SetName(const std::string& name)
        { mName = name; }
        void SetGraphOutputElementId(const std::string& id)
        { mSrcElemId = id; }
        void SetGraphOutputElementPort(const std::string& name)
        { mSrcElemPort = name; }
        std::size_t GetHash() const;
        std::string GetName() const
        { return mName; }
        std::string GetGraphOutputElementId() const
        { return mSrcElemId; }
        std::string GetGraphOutputElementPort() const
        { return mSrcElemPort; }
        std::string GetId() const
        { return mId; }
        std::size_t GetNumElements() const
        { return mElements.size(); }
        std::size_t GetNumLinks() const
        { return mLinks.size(); }
        const Element& GetElement(size_t index) const
        { return base::SafeIndex(mElements, index); }
        const Link& GetLink(size_t index) const
        { return base::SafeIndex(mLinks, index); }
        const Element* FindElementById(const std::string& id) const
        {
            return base::SafeFind(mElements, [&id](const auto& elem) {
                return elem.id == id;
            });
        }
        Element& GetElement(size_t index)
        { return base::SafeIndex(mElements, index); }
        Element* FindElementById(const std::string& id)
        {
            return base::SafeFind(mElements, [&id](const auto& elem) {
                return elem.id == id;
            });
        }
        const Link* FindLinkById(const std::string& id) const
        {
            return base::SafeFind(mLinks, [&id](const auto& link) {
                return link.id == id;
            });
        }
        Link& GetLink(size_t index)
        { return base::SafeIndex(mLinks, index); }
        Link* FindLinkById(const std::string& id)
        {
            return base::SafeFind(mLinks, [&id](const auto& link) {
                return link.id == id;
            });
        }
        std::unique_ptr<GraphClass> Copy() const
        { return std::make_unique<GraphClass>(*this); }
        std::unique_ptr<GraphClass> Clone() const
        {
            auto ret = std::make_unique<GraphClass>(*this);
            ret->mId = base::RandomString(10);
            return ret;
        }

        void IntoJson(data::Writer& writer) const;
        static std::optional<GraphClass> FromJson(const data::Reader& reader);
    private:
        std::string mName;
        std::string mId;
        std::string mSrcElemId;
        std::string mSrcElemPort;
        std::vector<Link> mLinks;
        std::vector<Element> mElements;
    };

    class Graph :  public Element
    {
    public:
        // Create new audio graph with the given human readable name.
        // The name will be audio stream name on the device, and will be
        // shown for example in Pavucontrol on Linux.
        Graph(const std::string& name, const std::string& id);
        Graph(const std::string& name);
        Graph(const GraphClass& klass);
        Graph(const std::string& name, const GraphClass& klass);
        Graph(std::shared_ptr<const GraphClass> klass);
        Graph(const std::string& name, std::shared_ptr<const GraphClass> klass);
        Graph(Graph&& other);
        Graph(const Graph&) = delete;
        // Add a new element to the graph. Note that the element is not yet
        // linked anywhere. You'll likely want to call LinkElement after this.
        // Returns pointer to the new element in the graph.
        template<typename ElementType>
        ElementType* AddElement(ElementType&& element)
        {
            auto e = std::make_unique<std::remove_reference_t<ElementType>>(std::forward<ElementType>(element));
            mElements.push_back(std::move(e));
            return static_cast<ElementType*>(mElements.back().get());
        }
        // Add a new element to the graph. Note that the element is not yet
        // linked anywhere. You'll likely want to call LinkElement after this.
        // Returns pointer to the new element in the graph.
        Element* AddElementPtr(std::unique_ptr<Element> element);
        // Find an element by the element id. If not found will return nullptr.
        Element* FindElementById(const std::string& id);
        // Find an element by the element name. If not found will return nullptr.
        Element* FindElementByName(const std::string& name);
        // Get the element at the given index. The index must be valid.
        Element& GetElement(size_t index)
        { return *base::SafeIndex(mElements, index).get(); }
        // Find an element by the element id. If not found will return nullptr.
        const Element* FindElementById(const std::string& id) const;
        // Find an element by the element name. If not found will return nullptr.
        const Element* FindElementByName(const std::string& name) const;
        // Get the element at the given index. The index must be valid.
        const Element& GetElement(size_t index) const
        { return *base::SafeIndex(mElements, index).get(); }

        // Link two elements together so that the source port on the
        // the source element is used as the buffer source for the
        // input on the destinations element's destination port.
        // No error checking is performed in terms of checking the
        // validity of the provided objects or the semantics of link
        // on the graph's validity.
        void LinkElements(Element* src_elem, Port* src_port,
                          Element* dst_elem, Port* dst_port);
        // Link the graph's output to a source element. Normally this
        // would be the tail element of your audio graph. I.e. in a graph
        // with elements A, B and C and edges A->B->C the output from C
        // would likely be linked to the graph. The graph will then source
        // the output buffer for the audio device from element C.
        void LinkGraph(Element* src_elem, Port* src_port);
        // Convenience function to link graph elements together using their
        // human readable names. Care should be taken to make sure that the
        // names are unique. In case of duplicated names the first matching
        // element/port is used.
        // Returns true if linking succeeded, i.e. all elements/ports by the
        // given names were found.
        bool LinkElements(const std::string& src_elem_name, const std::string& src_port_name,
                          const std::string& dst_elem_name, const std::string& dst_port_name);
        // Convenience function to link the graph to a source element.
        // Returns true if linking succeeded, i.e. the element and the port
        // by the given names were found.
        bool LinkGraph(const std::string& src_elem_name, const std::string& src_port_name);

        // Create a human readable description of the routes between
        // elements and their ports in the audio graph.
        std::vector<std::string> Describe() const;
        // Create a human readable description of the routes between
        // elements and their ports in the audio graph starting at
        // the given src element.
        std::vector<std::string> DescribePaths(const Element* src) const;

        // Get the number of elements in the graph.
        std::size_t GetNumElements() const
        { return mElements.size(); }

        // Find out the destination port for the given source port.
        // Returns nullptr if no such port mapping exists.
        Port* FindDstPort(const Port* src);
        // Find out the destination port for the given source port.
        // Returns nullptr if no such port mapping exists.
        const Port* FindDstPort(const Port* source) const;
        // Find out the src port for the given destination port.
        // Returns nullptr if no such port mapping exists.
        Port* FindSrcPort(const Port* dst);
        // Find out the src port for the given destination port.
        // Returns nullptr if no such port mapping exists.
        const Port* FindSrcPort(const Port* dst) const;

        // Find the element that owns the given input port. Returns nullptr
        // if no such element.
        Element* FindInputPortOwner(const Port* port);
        // Find the element that owns the given input port. Returns nullptr
        // if no such element.
        Element* FindOutputPortOwner(const Port* port);
        // Find the element that owns the given output port. Returns nullptr
        // if no such element.
        const Element* FindInputPortOwner(const Port* port) const;
        // Find the element that owns the given output port. Returns nullptr
        // if no such element.
        const Element* FindOutputPortOwner(const Port* port) const;

        // Return true if the there exist a source-destination mapping
        // for the given source port. I.e. this src port is already linked
        // with some destination port.
        bool IsSrcPortTaken(const Port* src) const;
        bool IsSrcPortTaken(const std::string& elem_name, const std::string& port_name) const;
        // Return true if there exists a source-destination mapping
        // for the given destination port. I.e. something is already
        // linked to this dst port.
        bool IsDstPortTaken(const Port* dst) const;

        bool HasElement(Element* element) const;

        Format GetFormat() const
        { return mFormat; }

        // Element implementation note that  GetName is already part of the Source impl
        virtual std::string GetId() const override
        { return mId; }
        virtual std::string GetName() const override
        { return mName; }
        virtual std::string GetType() const override
        { return "Graph"; }
        virtual bool IsSource() const override
        { return true; }
        virtual bool IsSourceDone() const override
        { return mDone; }
        virtual bool Prepare(const Loader& loader) override;
        virtual void Process(EventQueue& events, unsigned milliseconds) override;
        virtual void Shutdown() override;
        virtual void Advance(unsigned int ms) override;
        virtual unsigned GetNumOutputPorts() const override
        { return 1; }
        virtual Port& GetOutputPort(unsigned index) override
        {
            if (index == 0) return mPort;
            BUG("No such port index.");
        }
        virtual bool DispatchCommand(const std::string& dest, Element::Command& cmd) override;
        Graph& operator=(const Graph&) = delete;
    private:
        using AdjacencyList = std::unordered_set<Element*>;
        const std::string mName;
        const std::string mId;
        // Maps an element to its source elements, i.e. answers
        // the question "which elements does this element depend on?".
        std::unordered_map<Element*, AdjacencyList> mSrcMap;
        // Maps an element to its destination elements, i.e. answers
        // the question "which elements depend on this element?".
        std::unordered_map<Element*, AdjacencyList> mDstMap;
        // Maps source ports to destination ports.
        std::unordered_map<Port*, Port*> mPortMap;
        // The container of all elements in the graph
        std::vector<std::unique_ptr<Element>> mElements;
        // The schedule, i.e. topological order in which
        // the elements need to be operated on.
        std::vector<Element*> mTopoOrder;
        // The current output format as per determined from
        // the last element on the graph
        audio::Format mFormat;
        // The graph tail port out of which the graph pulls its data.
        SingleSlotPort mPort;
        // Flag to indicate when the graph is all done,
        // i.e. all elements are done and there are no
        // more queued buffers in ports.o
        bool mDone = false;
    };

    // Provide PCM encoded audio device data through the evaluation of
    // an audio graph that is comprised of audio elements. Each element
    // can take a stream of data as an input, modify it and then provide
    // an outgoing stream of data.
    class AudioGraph : public Source
    {
    public:
        AudioGraph(const std::string& name);
        AudioGraph(const std::string& name, Graph&& graph);
        AudioGraph(AudioGraph&& other);
        AudioGraph(const AudioGraph&) = delete;

        // Prepare the graph for playback. Prepare should be called
        // after all the elements have been added and linked to the graph
        // and before the graph is given to the audio device for playback.
        // The graph may not contain any cycles.
        // Returns true on success if all audio elements were prepared
        // successfully. After this it's possible to send the graph to
        // the audio device/player for playback.
        bool Prepare(const Loader& loader);

        // Source implementation.
        virtual unsigned GetRateHz() const noexcept override;
        virtual unsigned GetNumChannels() const noexcept override;
        virtual Source::Format GetFormat() const noexcept override;
        virtual std::string GetName() const noexcept override;
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        virtual bool HasMore(std::uint64_t num_bytes_read) const noexcept override;
        virtual bool Reset() noexcept override;
        virtual void RecvCommand(std::unique_ptr<Command> cmd) noexcept override;
        virtual std::unique_ptr<Event> GetEvent() noexcept override;

        // quick access to the underlying graph.
        Graph& GetGraph()
        { return mGraph; }
        Graph* operator->()
        { return &mGraph; }
        const Graph* operator->() const
        { return &mGraph; }
        const Graph& GetGraph() const
        { return mGraph; }

        AudioGraph& operator=(const AudioGraph&) = delete;

        static
        std::unique_ptr<audio::Command> MakeCommandPtr(const std::string& destination,
                                                       std::unique_ptr<Element::Command>&& cmd);

        template<typename GraphCommand> static
        std::unique_ptr<audio::Command> MakeCommand(const std::string& destination, GraphCommand&& cmd)
        {
            auto ret = Element::MakeCommand(std::forward<GraphCommand>(cmd));
            return MakeCommandPtr(destination, std::move(ret));
        }
    private:
        struct GraphCmd;
        const std::string mName;
        audio::Graph mGraph;
        audio::Format mFormat;
        audio::Element::EventQueue mEvents;
        std::uint64_t mMillisecs = 0;
        std::size_t mPendingOffset = 0;
        BufferHandle mPendingBuffer;
    };

} // namespace
