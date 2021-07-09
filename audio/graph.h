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
#include <unordered_map>
#include <unordered_set>

#include "base/utility.h"
#include "audio/source.h"
#include "audio/format.h"
#include "audio/element.h"

namespace audio
{
    // Provide PCM encoded audio device data through the evaluation of
    // an audio graph that is comprised of audio elements. Each element
    // can take a stream of data as an input, modify it and then provide
    // an outgoing stream of data.
    class Graph : public Source
    {
    public:
        // Creata new audio graph with the given human readable name.
        // The name will be audio stream name on the device, and will be
        // shown for example in Pavucontrol on Linux.
        Graph(const std::string& name);
        // dtor.
       ~Graph();
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

        // Find out the destination port for the given input/source port.
        // Returns nullptr if no such port mapping exists, otherwise
        // the destination port is returned.
        Port* FindDstPort(const Port* src);
        // Find out the destination port for the given input/source port.
        // Returns nullptr if no such port mapping exists, otherwise
        // the destination port is returned.
        const Port* FindDstPort(const Port* source) const;

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
        // Return true if there exists a source-destination mapping
        // for the given destination port. I.e. something is already
        // linked to this dst port.
        bool IsDstPortTaken(const Port* dst) const;

        // Prepare the graph for playback. Prepare should be called
        // after all the elements have been added and linked to the graph
        // and before the graph is given to the audio device for playback.
        // The graph may not contain any cycles.
        // Returns true on success if all audio elements were prepared
        // successfully. After this it's possible to send the graph to
        // the audio device/player for playback.
        bool Prepare();

        // Source implementation.
        virtual unsigned GetRateHz() const noexcept override;
        virtual unsigned GetNumChannels() const noexcept override;
        virtual Source::Format GetFormat() const noexcept override;
        virtual std::string GetName() const noexcept override;
        virtual unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        virtual bool HasNextBuffer(std::uint64_t num_bytes_read) const noexcept override;
        virtual bool Reset() noexcept override;
    private:
        using AdjacencyList = std::unordered_set<Element*>;
        std::string mName;
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
} // namespace
