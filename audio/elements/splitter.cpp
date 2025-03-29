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
#include "audio/elements/splitter.h"

namespace audio
{

Splitter::Splitter(std::string name, unsigned num_outs)
  : Splitter(std::move(name), base::RandomString(10), num_outs)
{}

Splitter::Splitter(std::string name, std::string id, unsigned num_outs)
  : mName(std::move(name))
  , mId(std::move(id))
  , mIn("in")
{
    ASSERT(num_outs);
    for (unsigned i=0; i<num_outs; ++i)
    {
        SingleSlotPort port(base::FormatString("out%1", i));
        mOuts.push_back(std::move(port));
    }
}
Splitter::Splitter(std::string name, std::string id, const std::vector<PortDesc>& outs)
  : mName(std::move(name))
  , mId(std::move(id))
  , mIn("in")
{
    for (const auto& out : outs)
    {
        SingleSlotPort port(out.name);
        mOuts.push_back(std::move(port));
    }
}

bool Splitter::Prepare(const Loader& loader, const PrepareParams& params)
{
    const auto& format = mIn.GetFormat();
    if (!IsValid(format))
    {
        ERROR("Audio splitter input format is invalid. [elem=%1, port=%2]", mName, mIn.GetName());
        return false;
    }
    for (auto& out : mOuts)
    {
        out.SetFormat(format);
    }
    DEBUG("Audio splitter prepared successfully. [elem=%1, format=%2]", format);
    return true;
}
void Splitter::Process(Allocator& allocator, EventQueue& events, unsigned milliseconds)
{
    TRACE_SCOPE("Splitter");

    BufferHandle src_buffer;
    if (!mIn.PullBuffer(src_buffer))
        return;

    for (auto& out : mOuts)
    {
        BufferHandle out_buffer = allocator.Allocate(src_buffer->GetByteSize());
        out_buffer->SetFormat(out_buffer->GetFormat());
        out_buffer->CopyData(*src_buffer);
        out_buffer->CopyInfoTags(*src_buffer);
        out.PushBuffer(out_buffer);
    }
}

} // namespace