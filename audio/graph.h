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
#include <cstdint>
#include <cstddef>

#include "audio/source.h"
#include "audio/format.h"
#include "audio/element.h"
#include "audio/command.h"
#include "audio/elements/graph.h"

namespace audio
{
    // Source implementation for an audio graph. Evaluates the
    // graph and its elements in order to produce PCM audio data.
    class AudioGraph : public Source
    {
    public:
        using PrepareParams = Graph::PrepareParams;

        explicit AudioGraph(AudioGraph&& other) noexcept;
        explicit AudioGraph(const std::string& name);
        AudioGraph(std::string name, Graph&& graph);
        AudioGraph(const AudioGraph&) = delete;

        // Prepare the graph for playback. Prepare should be called
        // after all the elements have been added and linked to the graph
        // and before the graph is given to the audio device for playback.
        // The graph may not contain any cycles.
        // Returns true when all audio elements were prepared successfully
        // and false on any error. After successful prepare it's possible
        // to send the graph to the audio player for playback.
        bool Prepare(const Loader& loader, const PrepareParams& params);

        // Source implementation.
        unsigned GetRateHz() const noexcept override;
        unsigned GetNumChannels() const noexcept override;
        Source::Format GetFormat() const noexcept override;
        std::string GetName() const noexcept override;
        unsigned FillBuffer(void* buff, unsigned max_bytes) override;
        bool HasMore(std::uint64_t num_bytes_read) const noexcept override;
        void Shutdown() noexcept override;
        void RecvCommand(std::unique_ptr<Command> cmd) noexcept override;
        std::unique_ptr<Event> GetEvent() noexcept override;

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
