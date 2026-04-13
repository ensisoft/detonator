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

#include "graphics/device.h"
#include "graphics/drawable_geometry.h"

namespace gfx
{
// static
DrawGeometryHandle DrawGeometryHandle::Null;

// static
DrawGeometryBuffer DrawGeometryBuffer::Null;

// static
DrawGeometryHandle DrawGeometryHandle::CreateErrorGeometry(const std::string& id,
    std::string message, std::string name, size_t content_hash, Device& device)
{
    Geometry::CreateArgs args;
    args.usage        = BufferUsage::Static;
    args.content_hash = content_hash;
    args.content_name = std::move(name);
    args.error_log    = std::move(message);
    args.fallback     = true;
    return device.CreateGeometry(id, std::move(args));
}

} // namespace