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
#include "base/utility.h"
#include "audio/elements/delay.h"

namespace audio
{

Delay::Delay(std::string name, std::string id, unsigned delay)
  : mName(std::move(name))
  , mId(std::move(id))
  , mIn("in")
  , mOut("out")
  , mDelay(delay)
{}

Delay::Delay(std::string name, unsigned delay)
  : Delay(std::move(name), base::RandomString(10), delay)
{}

bool Delay::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto& format = mIn.GetFormat();
    mOut.SetFormat(format);
    DEBUG("Audio delay element prepared successfully. [elem=%1, output=%2]", mName, format);
    return true;
}
void Delay::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Delay");

    if (mDelay > 0u)
        return;

    BufferHandle buffer;
    if (mIn.PullBuffer((buffer)))
        mOut.PushBuffer(buffer);
}

void Delay::Advance(unsigned milliseconds)
{
    const auto min = std::min(mDelay, milliseconds);
    mDelay -= min;
}

} // namespace