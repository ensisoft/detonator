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

namespace gfx::glsl {
// vertex (drawable) shaders
extern const char* vertex_base;
extern const char* vertex_3d_perceptual;
extern const char* vertex_3d_model;
extern const char* vertex_3d_simple;
extern const char* vertex_2d_simple;
extern const char* vertex_2d_effect;
extern const char* vertex_2d_particle;
extern const char* vertex_2d_point_tile;
extern const char* vertex_2d_quad_tile;

// uber shader program shader core both vertex+fragment
extern const char* vertex_main_generic;
extern const char* fragment_main_generic;
extern const char* fragment_basic_light;
extern const char* fragment_basic_fog;

// fragment (material) shaders
extern const char* fragment_base;
extern const char* fragment_color_shader;
extern const char* fragment_gradient_shader;
extern const char* fragment_sprite_shader;
extern const char* fragment_texture_shader;
extern const char* fragment_tilemap_shader;
extern const char* fragment_particle_2d_shader;
extern const char* fragment_basic_light_shader;
extern const char* fragment_bitmap_text_shader;
extern const char* fragment_texture_text_shader;

// fragment utility
extern const char* fragment_texture_functions;

// fragment kernels
extern const char* fragment_blur_kernel;

// generic type agnostic shader code
extern const char* srgb_funcs;

} // namespace