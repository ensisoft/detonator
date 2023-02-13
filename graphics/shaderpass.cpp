// Copyright (C) 2020-2023 Sami Väisänen
// Copyright (C) 2020-2023 Ensisoft http://www.ensisoft.com
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

#include "config.h"

#include "graphics/shaderpass.h"
#include "graphics/device.h"

namespace gfx {
std::string ShaderPass::ModifyVertexSource(Device& device, std::string source) const
{
    return source;
}

std::string ShaderPass::ModifyFragmentSource(Device& device, std::string source) const
{
    constexpr auto* src = R"(
vec4 ShaderPass(vec4 color) {
    return color;
}
)";
    source += src;
    return source;
}

} // namespace
