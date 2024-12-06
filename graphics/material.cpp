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

//                  == Notes about shaders ==
// 1. Shaders are specific to a device within compatibility constraints
// but a GLES 2 device context cannot use GL ES 3 shaders for example.
//
// 2. Logically shaders are part of the higher level graphics system, i.e.
// they express some particular algorithm/technique in a device dependant
// way, so logically they belong to the higher layer (i.e. roughly painter
// material level of abstraction).
//
// Basically this means that the device abstraction breaks down because
// we need this device specific (shader) code that belongs to the higher
// level of the system.
//
// A fancy way of solving this would be to use a shader translator, i.e.
// a device that can transform shader from one language version to another
// language version or even from one shader language to another, for example
// from GLSL to HLSL/MSL/SPIR-V.  In fact this is exactly what libANGLE does
// when it implements GL ES2/3 on top of DX9/11/Vulkan/GL/Metal.
// But this only works when the particular underlying implementations can
// support similar feature sets. For example instanced rendering is not part
// of ES 2 but >= ES 3 so therefore it's not possible to translate ES3 shaders
// using instanced rendering into ES2 shaders directly but the whole rendering
// path must be changed to not use instanced rendering.
//
// This then means that if a system was using a shader translation layer (such
// as libANGLE) any decent system would still require several different rendering
// paths. For example a primary "high end path" and a "fallback path" for low end
// devices. These different paths would use different feature sets (such as instanced
// rendering) and also (in many cases) require different shaders to be written to
// fully take advantage of the graphics API features.
// Then these two different rendering paths would/could use a shader translation
// layer in order to use some specific graphics API. (through libANGLE)
//
//            <<Device>>
//              |    |
//     <ES2 Device> <ES3 Device>
//             |      |
//            <libANGLE>
//                 |
//      [GL/DX9/DX11/VULKAN/Metal]
//
// Where "ES2 Device" provides the low end graphics support and "ES3 Device"
// device provides the "high end" graphics rendering path support.
// Both device implementations would require their own shaders which would then need
// to be translated to the device specific shaders matching the feature level.
//
// Finally, who should own the shaders and who should edit them ?
//   a) The person who is using the graphics library ?
//   b) The person who is writing the graphics library ?
//
// The answer seems to be mostly the latter (b), i.e. in most cases the
// graphics functionality should work "out of the box" and the graphics
// library should *just work* without the user having to write shaders.
// However there's a need that user might want to write their own special
// shaders because they're cool to do so and want some special customized
// shader effect.
//

