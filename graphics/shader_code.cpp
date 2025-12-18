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

#include "graphics/shader_code.h"

namespace gfx::glsl {

    const char* vertex_base = {
#include "shaders/vertex_shader_base.glsl"
    };

    const char* vertex_3d_perceptual = {
#include "shaders/vertex_perceptual_3d_shader.glsl"
    };

    const char* vertex_3d_model = {
#include "shaders/vertex_3d_model_shader.glsl"
    };

    const char* vertex_3d_simple = {
#include "shaders/vertex_3d_simple_shader.glsl"
    };

    const char* vertex_2d_simple = {
#include "shaders/vertex_2d_simple_shader.glsl"
    };

    const char* vertex_2d_effect = {
#include "shaders/vertex_2d_effect.glsl"
    };

    const char* vertex_2d_particle = {
#include "shaders/vertex_2d_particle_shader.glsl"
    };

    const char* vertex_2d_point_tile = {
#include "shaders/vertex_tilebatch_point_shader.glsl"
    };

    const char* vertex_2d_quad_tile = {
#include "shaders/vertex_tilebatch_quad_shader.glsl"
    };

    const char* vertex_main_generic = {
#include "shaders/generic_main_vertex_shader.glsl"
    };

    const char* fragment_main_generic = {
#include "shaders/generic_main_fragment_shader.glsl"
};

    const char* fragment_basic_light = {
#include "shaders/basic_light.glsl"
    };

    const char* fragment_basic_fog = {
#include "shaders/basic_fog.glsl"
    };

    const char* fragment_base = {
#include "shaders/fragment_shader_base.glsl"
    };

    const char* fragment_color_shader = {
#include "shaders/fragment_color_shader.glsl"
    };

    const char* fragment_gradient_shader = {
#include "shaders/fragment_gradient_shader.glsl"
    };

    const char* fragment_sprite_shader = {
#include "shaders/fragment_sprite_shader.glsl"
    };

    const char* fragment_texture_shader =  {
#include "shaders/fragment_texture_shader.glsl"
    };

    const char* fragment_tilemap_shader = {
#include "shaders/fragment_tilemap_shader.glsl"
    };

    const char* fragment_particle_2d_shader = {
#include "shaders/fragment_2d_particle_shader.glsl"
    };

    const char* fragment_basic_light_shader = {
#include "shaders/fragment_basic_light_material_shader.glsl"
    };

    const char* fragment_bitmap_text_shader = {
#include "shaders/fragment_text_bitmap_shader.glsl"
    };

    const char* fragment_texture_text_shader = {
#include "shaders/fragment_text_texture_shader.glsl"
    };

    const char* fragment_texture_functions =  {
#include "shaders/fragment_texture_functions.glsl"
    };

    const char* fragment_blur_kernel = {
#include "shaders/fragment_blur_kernel.glsl"
    };

    const char* srgb_funcs = {
#include "shaders/srgb_functions.glsl"
    };

} // namespace