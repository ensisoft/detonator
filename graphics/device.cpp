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

#include "config.h"

#include "graphics/device.h"

namespace gfx
{
// WebGL 1.0 is based on OpenGL ES 2.0 and is very similar with just some
// differences. so right now we're creating an ES2 device for WebGL1

// static
std::shared_ptr<Device> Device::Create(std::shared_ptr<Device::Context> context)
{
    if (context->GetVersion() == Device::Context::Version::OpenGL_ES2)
        return detail::CreateOpenGLES2Device(context);
    else if (context->GetVersion() == Device::Context::Version::WebGL_1)
        return detail::CreateOpenGLES2Device(context);
    return nullptr;
}
// static
std::shared_ptr<Device> Device::Create(Device::Context* context)
{
    if (context->GetVersion() == Device::Context::Version::OpenGL_ES2)
        return detail::CreateOpenGLES2Device(context);
    else if (context->GetVersion() == Device::Context::Version::WebGL_1)
        return detail::CreateOpenGLES2Device(context);
    return nullptr;
}

} // namespace