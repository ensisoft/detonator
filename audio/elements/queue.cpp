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
#include "audio/elements/queue.h"

namespace audio
{

bool Queue::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto format = mIn.GetFormat();
    mOut.SetFormat(format);
    DEBUG("Audio queue element prepared successfully. [elem=%1, output=%2]", mName, format);
    return true;
}

void Queue::Process(Allocator& allocator, EventQueue& events, unsigned int milliseconds)
{
    TRACE_SCOPE("Queue");

    BufferHandle buffer;
    if (mIn.PullBuffer(buffer))
        mQueue.push(std::move(buffer));

    if (mQueue.empty())
        return;
    if (mOut.IsFull())
        return;

    if (mOut.PushBuffer(mQueue.front()))
        mQueue.pop();
}


} // namespace