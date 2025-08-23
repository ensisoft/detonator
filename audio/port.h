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

#pragma once

#include "config.h"

#include <string>
#include <vector>

#include "audio/buffer.h"
#include "audio/format.h"

namespace audio
{
    struct PortDesc {
        std::string name;
    };

    struct PortControlMessage {
        std::string message;
    };

    // Port provides an input/output abstraction for connecting
    // elements and their output ports to input ports.
    // A port is used to push and pull data buffers in and out.
    // Each port additionally specifies the format that it supports
    // and understand.
    class Port
    {
    public:
        using BufferHandle = audio::BufferHandle;
        using Format = audio::Format;

        explicit Port(std::string name) noexcept
          : mName(std::move(name))
        {}
        Port(const Port& other) = default;
        Port() = default;
        // Push a new buffer for into the port.
        // The audio graph will *pull* from the source output ports
        // and *push* into the destination input ports.
        // An element will *pull* from its input ports and *push*
        // into its output ports.
        // Returns false if the queue overflowed and buffer could
        // not be pushed.
        bool PushBuffer(BufferHandle buffer) noexcept
        {
            if (mBuffer)
                return false;
            mBuffer = std::move(buffer);
            return true;
        }

        // Pull a buffer out of the port.
        // The audio graph will *pull* from the source output ports
        // and *push* into the destination input ports.
        // An element will *pull* from its input ports and *push*
        // into its output ports.
        // Returns false if the port was empty and no buffer was available.
        bool PullBuffer(BufferHandle& buffer) noexcept
        {
            if (!mBuffer)
                return false;
            buffer = std::move(mBuffer);
            mBuffer = BufferHandle {};
            return true;
        }

        inline bool HasMessages() const noexcept
        { return !mMessages.empty(); }

        inline void TransferMessages(std::vector<PortControlMessage>* out) noexcept
        { *out = std::move(mMessages); }

        inline void PushMessage(PortControlMessage message) noexcept
        { mMessages.push_back(std::move(message)); }

        // Get the human -readable name of the port.
        inline std::string GetName() const
        { return mName; }

        // Get the port's data format. The format is undefined until
        // the whole audio graph has been prepared.
        inline  Format GetFormat() const noexcept
        { return mFormat; }

        // Set the result of the port format negotiation.
        inline void SetFormat(const Format& format) noexcept
        { mFormat = format; }

        // Perform format compatibility check by checking against the given
        // suggested audio stream format. Should return true if the format
        // is accepted or false to indicate that the format is not supported.
        inline bool CanAccept(const Format& format) const noexcept
        {
            return true;
        }

        // Return true if there are pending buffers in the port's buffer queue.
        inline bool HasBuffers() const noexcept
        {
            return mBuffer ? true : false;
        }

        // Return true if the port is full and cannot queue more.
        inline bool IsFull() const noexcept
        {
            return mBuffer ? true : false;
        }
    private:
        std::string mName;
        std::vector<PortControlMessage> mMessages;
        Format mFormat;
        BufferHandle mBuffer;
    };

    using SingleSlotPort = Port;

} // namespace