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


#include "editor/app/utility.h"
#include "editor/app/project_settings.h"

namespace app
{
void IntoJson(QJsonObject& project, const ProjectSettings& settings)
{
    JsonWrite(project, "multisample_sample_count",  settings.multisample_sample_count);
    JsonWrite(project, "application_identifier"  ,  settings.game_identifier);
    JsonWrite(project, "application_name"        ,  settings.game_name);
    JsonWrite(project, "application_version"     ,  settings.game_version);
    JsonWrite(project, "application_library_win" ,  settings.game_engine_library_win);
    JsonWrite(project, "application_library_lin" ,  settings.game_engine_library_lin);
    JsonWrite(project, "application_library_wasm",  settings.game_engine_library_wasm);
    JsonWrite(project, "loading_screen_font"     ,  settings.loading_font);
    JsonWrite(project, "debug_font"              ,  settings.debug_font);
    JsonWrite(project, "debug_show_fps"          ,  settings.debug_show_fps);
    JsonWrite(project, "debug_show_msg"          ,  settings.debug_show_msg);
    JsonWrite(project, "debug_draw"              ,  settings.debug_draw);
    JsonWrite(project, "debug_print_fps"         ,  settings.debug_print_fps);
    JsonWrite(project, "logging_debug"           ,  settings.log_debug);
    JsonWrite(project, "logging_warn"            ,  settings.log_debug);
    JsonWrite(project, "logging_info"            ,  settings.log_debug);
    JsonWrite(project, "logging_error"           ,  settings.log_debug);
    JsonWrite(project, "default_min_filter"      ,  settings.default_min_filter);
    JsonWrite(project, "default_mag_filter"      ,  settings.default_mag_filter);
    JsonWrite(project, "webgl_power_preference"  ,  settings.webgl_power_preference);
    JsonWrite(project, "webgl_antialias"         ,  settings.webgl_antialias);
    JsonWrite(project, "html5_developer_ui"      ,  settings.html5_developer_ui);
    JsonWrite(project, "html5_pointer_lock"      ,  settings.html5_pointer_lock);
    JsonWrite(project, "canvas_mode"             ,  settings.canvas_mode);
    JsonWrite(project, "canvas_fs_strategy"      ,  settings.canvas_fs_strategy);
    JsonWrite(project, "canvas_width"            ,  settings.canvas_width);
    JsonWrite(project, "canvas_height"           ,  settings.canvas_height);
    JsonWrite(project, "window_mode"             ,  settings.window_mode);
    JsonWrite(project, "window_width"            ,  settings.window_width);
    JsonWrite(project, "window_height"           ,  settings.window_height);
    JsonWrite(project, "window_can_resize"       ,  settings.window_can_resize);
    JsonWrite(project, "window_has_border"       ,  settings.window_has_border);
    JsonWrite(project, "window_vsync"            ,  settings.window_vsync);
    JsonWrite(project, "window_cursor"           ,  settings.window_cursor);
    JsonWrite(project, "config_srgb"             ,  settings.config_srgb);
    JsonWrite(project, "grab_mouse"              ,  settings.grab_mouse);
    JsonWrite(project, "save_window_geometry"    ,  settings.save_window_geometry);
    JsonWrite(project, "ticks_per_second"        ,  settings.ticks_per_second);
    JsonWrite(project, "updates_per_second"      ,  settings.updates_per_second);
    JsonWrite(project, "working_folder"          ,  settings.working_folder);
    JsonWrite(project, "command_line_arguments"  ,  settings.command_line_arguments);
    JsonWrite(project, "game_home"               ,  settings.game_home);
    JsonWrite(project, "use_gamehost_process"    ,  settings.use_gamehost_process);
    JsonWrite(project, "enable_physics"          ,  settings.enable_physics);
    JsonWrite(project, "num_velocity_iterations" ,  settings.num_velocity_iterations);
    JsonWrite(project, "num_position_iterations" ,  settings.num_position_iterations);
    JsonWrite(project, "phys_gravity_x"          ,  settings.physics_gravity.x);
    JsonWrite(project, "phys_gravity_y"          ,  settings.physics_gravity.y);
    JsonWrite(project, "phys_scale_x"            ,  settings.physics_scale.x);
    JsonWrite(project, "phys_scale_y"            ,  settings.physics_scale.y);
    JsonWrite(project, "game_viewport_width"     ,  settings.viewport_width);
    JsonWrite(project, "game_viewport_height"    ,  settings.viewport_height);
    JsonWrite(project, "clear_color"             ,  settings.clear_color);
    JsonWrite(project, "mouse_pointer_material"  ,  settings.mouse_pointer_material);
    JsonWrite(project, "mouse_pointer_drawable"  ,  settings.mouse_pointer_drawable);
    JsonWrite(project, "mouse_pointer_visible"   ,  settings.mouse_pointer_visible);
    JsonWrite(project, "mouse_pointer_hotspot"   ,  settings.mouse_pointer_hotspot);
    JsonWrite(project, "mouse_pointer_size"      ,  settings.mouse_pointer_size);
    JsonWrite(project, "mouse_pointer_units"     ,  settings.mouse_pointer_units);
    JsonWrite(project, "game_script"             ,  settings.game_script);
    JsonWrite(project, "audio_channels"          ,  settings.audio_channels);
    JsonWrite(project, "audio_sample_rate"       ,  settings.audio_sample_rate);
    JsonWrite(project, "audio_sample_type"       ,  settings.audio_sample_type);
    JsonWrite(project, "audio_buffer_size"       ,  settings.audio_buffer_size);
    JsonWrite(project, "enable_audio_pcm_caching",  settings.enable_audio_pcm_caching);
    JsonWrite(project, "desktop_audio_io_strategy", settings.desktop_audio_io_strategy);
    JsonWrite(project, "wasm_audio_io_strategy"  ,  settings.wasm_audio_io_strategy);
    JsonWrite(project, "preview_entity_script"   ,  settings.preview_entity_script);
    JsonWrite(project, "preview_scene_script"    ,  settings.preview_scene_script);
    JsonWrite(project, "preview_ui_script"       ,  settings.preview_ui_script);
}

void FromJson(const QJsonObject& project, ProjectSettings& settings)
{
    JsonReadSafe(project, "multisample_sample_count"  , &settings.multisample_sample_count);
    JsonReadSafe(project, "application_identifier"    , &settings.game_identifier);
    JsonReadSafe(project, "application_name"          , &settings.game_name);
    JsonReadSafe(project, "application_version"       , &settings.game_version);
    JsonReadSafe(project, "application_library_win"   , &settings.game_engine_library_win);
    JsonReadSafe(project, "application_library_lin"   , &settings.game_engine_library_lin);
    JsonReadSafe(project, "application_library_wasm"  , &settings.game_engine_library_wasm);
    JsonReadSafe(project, "loading_screen_font"       , &settings.loading_font);
    JsonReadSafe(project, "debug_font"                , &settings.debug_font);
    JsonReadSafe(project, "debug_show_fps"            , &settings.debug_show_fps);
    JsonReadSafe(project, "debug_show_msg"            , &settings.debug_show_msg);
    JsonReadSafe(project, "debug_draw"                , &settings.debug_draw);
    JsonReadSafe(project, "debug_print_fps"           , &settings.debug_print_fps);
    JsonReadSafe(project, "logging_debug"             , &settings.log_debug);
    JsonReadSafe(project, "logging_warn"              , &settings.log_debug);
    JsonReadSafe(project, "logging_info"              , &settings.log_debug);
    JsonReadSafe(project, "logging_error"             , &settings.log_debug);
    JsonReadSafe(project, "default_min_filter"        , &settings.default_min_filter);
    JsonReadSafe(project, "default_mag_filter"        , &settings.default_mag_filter);
    JsonReadSafe(project, "webgl_power_preference"    , &settings.webgl_power_preference);
    JsonReadSafe(project, "webgl_antialias"           , &settings.webgl_antialias);
    JsonReadSafe(project, "html5_developer_ui"        , &settings.html5_developer_ui);
    JsonReadSafe(project, "html5_pointer_lock"        , &settings.html5_pointer_lock);
    JsonReadSafe(project, "canvas_mode"               , &settings.canvas_mode);
    JsonReadSafe(project, "canvas_fs_strategy"        , &settings.canvas_fs_strategy);
    JsonReadSafe(project, "canvas_width"              , &settings.canvas_width);
    JsonReadSafe(project, "canvas_height"             , &settings.canvas_height);
    JsonReadSafe(project, "window_mode"               , &settings.window_mode);
    JsonReadSafe(project, "window_width"              , &settings.window_width);
    JsonReadSafe(project, "window_height"             , &settings.window_height);
    JsonReadSafe(project, "window_can_resize"         , &settings.window_can_resize);
    JsonReadSafe(project, "window_has_border"         , &settings.window_has_border);
    JsonReadSafe(project, "window_vsync"              , &settings.window_vsync);
    JsonReadSafe(project, "window_cursor"             , &settings.window_cursor);
    JsonReadSafe(project, "config_srgb"               , &settings.config_srgb);
    JsonReadSafe(project, "grab_mouse"                , &settings.grab_mouse);
    JsonReadSafe(project, "save_window_geometry"      , &settings.save_window_geometry);
    JsonReadSafe(project, "ticks_per_second"          , &settings.ticks_per_second);
    JsonReadSafe(project, "updates_per_second"        , &settings.updates_per_second);
    JsonReadSafe(project, "working_folder"            , &settings.working_folder);
    JsonReadSafe(project, "game_home"                 , &settings.game_home);
    JsonReadSafe(project, "command_line_arguments"    , &settings.command_line_arguments);
    JsonReadSafe(project, "use_gamehost_process"      , &settings.use_gamehost_process);
    JsonReadSafe(project, "enable_physics"            , &settings.enable_physics);
    JsonReadSafe(project, "num_position_iterations"   , &settings.num_position_iterations);
    JsonReadSafe(project, "num_velocity_iterations"   , &settings.num_velocity_iterations);
    JsonReadSafe(project, "phys_gravity_x"            , &settings.physics_gravity.x);
    JsonReadSafe(project, "phys_gravity_y"            , &settings.physics_gravity.y);
    JsonReadSafe(project, "phys_scale_x"              , &settings.physics_scale.x);
    JsonReadSafe(project, "phys_scale_y"              , &settings.physics_scale.y);
    JsonReadSafe(project, "game_viewport_width"       , &settings.viewport_width);
    JsonReadSafe(project, "game_viewport_height"      , &settings.viewport_height);
    JsonReadSafe(project, "clear_color"               , &settings.clear_color);
    JsonReadSafe(project, "mouse_pointer_material"    , &settings.mouse_pointer_material);
    JsonReadSafe(project, "mouse_pointer_drawable"    , &settings.mouse_pointer_drawable);
    JsonReadSafe(project, "mouse_pointer_visible"     , &settings.mouse_pointer_visible);
    JsonReadSafe(project, "mouse_pointer_hotspot"     , &settings.mouse_pointer_hotspot);
    JsonReadSafe(project, "mouse_pointer_size"        , &settings.mouse_pointer_size);
    JsonReadSafe(project, "mouse_pointer_units"       , &settings.mouse_pointer_units);
    JsonReadSafe(project, "game_script"               , &settings.game_script);
    JsonReadSafe(project, "audio_channels"            , &settings.audio_channels);
    JsonReadSafe(project, "audio_sample_rate"         , &settings.audio_sample_rate);
    JsonReadSafe(project, "audio_sample_type"         , &settings.audio_sample_type);
    JsonReadSafe(project, "audio_buffer_size"         , &settings.audio_buffer_size);
    JsonReadSafe(project, "enable_audio_pcm_caching"  , &settings.enable_audio_pcm_caching);
    JsonReadSafe(project, "desktop_audio_io_strategy" , &settings.desktop_audio_io_strategy);
    JsonReadSafe(project, "wasm_audio_io_strategy"    , &settings.wasm_audio_io_strategy);
    JsonReadSafe(project, "preview_entity_script"     , &settings.preview_entity_script);
    JsonReadSafe(project, "preview_scene_script"      , &settings.preview_scene_script);
    JsonReadSafe(project, "preview_ui_script"         , &settings.preview_ui_script);
}

} // namespace