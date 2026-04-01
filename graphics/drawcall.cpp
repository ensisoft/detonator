// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
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

#include "graphics/drawcall.h"
#include "graphics/device.h"

namespace gfx
{

InstanceDataPtr PrepareDrawCall(const GenericInstancedDraw& draw, Device& device, bool editing_mode)
{
    InstanceData::CreateArgs args;
    args.content_hash = 0; // doesn't matter since we're not using the hash value for anything.
    args.content_name = draw.mContentName;
    args.usage = BufferUsage::Stream;

    args.buffer.SetInstanceDataLayout(gfx::GetInstanceDataLayout<InstanceAttribute>());
    args.buffer.Resize(draw.mInstances.size());
    for (size_t i=0; i<draw.mInstances.size(); ++i)
    {
        const auto& instance = draw.mInstances[i];
        InstanceAttribute ia;
        ia.iaModelVectorX = ToVec(instance.model_to_world[0]);
        ia.iaModelVectorY = ToVec(instance.model_to_world[1]);
        ia.iaModelVectorZ = ToVec(instance.model_to_world[2]);
        ia.iaModelVectorW = ToVec(instance.model_to_world[3]);
        args.buffer.SetInstanceData(ia, i);
    }

    return device.CreateInstanceData(draw.mContentId, std::move(args));
}
} // namespace