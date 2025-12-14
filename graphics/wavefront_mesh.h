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

#include "warnpush.h"
#include "warnpop.h"

#include <string>

#include "graphics/drawable.h"
#include "graphics/vertex.h"

namespace gfx
{
    // This class is designed to load simple GPU geometries from Wavefront .obj files.
    // Has minimal error checking, minimal functionality in terms of dealing with
    // .obj data (i.e. there's no triangulation, no normal computation etc).
    // Designed only to support some use cases in the editor where some models
    // are needed and those models are too cumbersome to create programmatically.
    class WavefrontMesh : public Drawable
    {
    public:
        explicit WavefrontMesh(std::string file_uri) : mFileUri(std::move(file_uri))
        {}
        WavefrontMesh() = default;
        void SetFileUri(std::string file_uri) noexcept
        { mFileUri = std::move(file_uri); }

        bool ApplyDynamicState(const Environment &env, Device &device, ProgramState &program, RasterState &state) const override;
        bool Construct(const Environment &env, Device &device, Geometry::CreateArgs &geometry) const override;
        ShaderSource GetShader(const Environment &env, const Device &device) const override;
        std::string GetShaderId(const Environment &env) const override;
        std::string GetShaderName(const Environment &env) const override;
        std::string GetGeometryId(const Environment &env) const override;

        SpatialMode GetSpatialMode() const override;
        DrawPrimitive GetDrawPrimitive() const override;
        Type GetType() const override;
    private:
        std::string mFileUri;
    };
} // namespace