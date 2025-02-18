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
#  include <QString>
#  include <QColor>
#  include <QJsonObject>
#  include <glm/vec2.hpp>
#include "warnpop.h"

#include "base/platform.h"
#include "engine/loader.h"
#include "graphics/device.h"

namespace app
{
     // Project settings.
    struct ProjectSettings {
        enum class WindowMode {
            // Use fullscreen rendering surface (it's still a window but
            // conceptually slightly different)
            // The size of the rendering surface will be determined
            // by the resolution of the display.
            Fullscreen,
            // Use a window of specific rendering surface size
            // border and resize settings.
            Windowed,
        };
        // sample count when using multi-sampled render targets.
        unsigned multisample_sample_count = 4;
        // Unique identifier for the project.
        QString application_identifier;
        // user defined name of the application.
        QString application_name;
        // User defined version of the application.
        QString application_version;
        // The library (.so or .dll) that contains the application
        // entry point and game::App implementation.
        QString application_library_lin = "app://libGameEngine.so";
        QString application_library_win = "app://GameEngine.dll";
        QString GetApplicationLibrary() const
        {
        #if defined(POSIX_OS)
            return application_library_lin;
        #else
            return application_library_win;
        #endif
        }
        void SetApplicationLibrary(QString library)
        {
        #if defined(POSIX_OS)
            application_library_lin = library;
        #else
            application_library_win = library;
        #endif
        }
        // Loading screen font
        QString loading_font = "app://fonts/ethnocentric rg.otf";

        // Debug font (if any) used by the engine to print debug messages.
        QString debug_font;
        bool debug_show_fps = false;
        bool debug_show_msg = false;
        bool debug_draw = false;
        bool debug_print_fps = false;
        // logging flags. may or may not be overridden by some UI/interface
        // when running/launching the game. For example the GameMain may provide
        // command line flags to override these settings.
        bool log_debug = false;
        bool log_warn  = true;
        bool log_info  = true;
        bool log_error = true;

        // How the HTML5 canvas is presented on the page
        enum class CanvasPresentationMode {
            // Canvas is presented as a normal HTML element among other elements.
            Normal,
            // Canvas is presented in fullscreen mode. Fullscreen strategy applies.
            FullScreen
        };
        // How the canvas is presented in fullscreen mode
        enum class CanvasFullScreenStrategy {
            // the canvas element is resized to take up all the possible space
            // on the page. (in its client area)
            SoftFullScreen,
            // The canvas element is presented in a "true" fullscreen experience
            // taking over the whole screen
            RealFullScreen
        };

        CanvasPresentationMode canvas_mode = CanvasPresentationMode::Normal;
        CanvasFullScreenStrategy canvas_fs_strategy = CanvasFullScreenStrategy::SoftFullScreen;
        // WebGL power preference.
        enum class PowerPreference {
            // Request a default power preference setting
            Default,
            // Request a low power mode that prioritizes power saving and battery over render perf
            LowPower,
            // Request a high performance mode that prioritizes rendering perf
            // over battery life/power consumption
            HighPerf
        };
        PowerPreference webgl_power_preference = PowerPreference::HighPerf;
        // HTML5 WebGL canvas render target width.
        unsigned canvas_width = 1024;
        // HTML5 WebGL canvas render target height
        unsigned canvas_height= 768;
        // WebGL doesn't have a specific MSAA/AA setting, only a boolean flag
        bool webgl_antialias = true;
        // flag to control HTML5 developer UI
        bool html5_developer_ui = false;
        // flag to control HTML5 pointer lol locking
        bool html5_pointer_lock = false;
        // default texture minification filter.
        gfx::Device::MinFilter default_min_filter = gfx::Device::MinFilter::Trilinear;
        // default texture magnification filter.
        gfx::Device::MagFilter default_mag_filter = gfx::Device::MagFilter::Linear;
        // The starting window mode.
        WindowMode window_mode = WindowMode::Windowed;
        // the assumed window width when launching the application
        // with its own window, i.e. when window_mode is Windowed.
        unsigned window_width = 1024;
        // the assumed window height  when launching the application
        // with its own window.
        unsigned window_height = 768;
        // window flag to allow window to be resized.
        bool window_can_resize = true;
        // window flag to control window border
        bool window_has_border = true;
        // vsync or not.
        bool window_vsync = false;
        // whether to use/show the native window system mouse cursor/pointer.
        bool window_cursor = true;
        // whether to use sRGB color space or not (not using sRGB implies linear)
        bool config_srgb = true;
        // flag to indicate whether the mouse should be
        // grabbed and confined within the window.
        bool grab_mouse = false;
        // Flag to indicate whether to save and restore the window
        // geometry between application runs.
        bool save_window_geometry = false;
        // how many times the app ticks per second.
        unsigned ticks_per_second = 1;
        // how many times the app updates per second.
        unsigned updates_per_second = 60;
        // Working folder when playing the game in the editor.
        QString working_folder = "${workspace}";
        // Arguments for when playing the game in the editor.
        QString command_line_arguments;
        // Game home when playing the game in the editor.
        QString game_home = "${game-home}-dev";
        // Use a separate game host process for playing the app.
        // Using a separate process will protect the editor
        // process from errors in the game app but it might
        // make debugging the game app more complicated.
        bool use_gamehost_process = true;
        // physics settings
        bool enable_physics = true;
        // number of velocity iterations per physics simulation step.
        unsigned num_velocity_iterations = 8;
        // number of position iterations per physics simulation step.
        unsigned num_position_iterations = 3;
        // gravity vector for physics simulation
        glm::vec2 physics_gravity = {0.0f, 10.0f};
        // scaling factor for mapping game world to physics world and back
        glm::vec2 physics_scale = {100.0f, 100.0f};
        // game's logical viewport width. this is *not* the final
        // viewport (game decides that) but only for visualization
        // in the editor.
        unsigned viewport_width = 1024;
        // game's logical viewport height. this is *not* the final
        // viewport (game decides that) but only for visualization
        // in the editor.
        unsigned viewport_height = 768;
        // The default engine clear color.
        QColor clear_color = {0x23, 0x23, 0x23, 255};
        // Whether the game should render a custom mouse pointer or not.
        bool mouse_pointer_visible = true;
        // What is the drawable shape of the game's custom mouse pointer.
        QString mouse_pointer_drawable = "_arrow_cursor";
        // What is the material of the game's custom mouse pointer.
        QString mouse_pointer_material = "_silver";
        // Where is the custom mouse pointer hotspot. (Normalized)
        glm::vec2 mouse_pointer_hotspot = {0.0f, 0.0f};
        // What is the pixel size of the game's custom mouse pointer.
        glm::vec2 mouse_pointer_size = {20.0f, 20.0f};
        enum class MousePointerUnits {
            Pixels, Units
        };
        // what are the units for the mouse pointer size.
        MousePointerUnits mouse_pointer_units = MousePointerUnits::Pixels;
        // name of the game's main script
        QString game_script = "ws://lua/game.lua";
        // Audio PCM data type.
        audio::SampleType audio_sample_type = audio::SampleType::Float32;
        // Number of audio output channels. 1 = monoaural, 2 stereo.
        audio::Channels audio_channels = audio::Channels::Stereo;
        // Audio output sample rate.
        unsigned audio_sample_rate = 44100;
        // Expected approximate audio buffer size in milliseconds.
        unsigned audio_buffer_size = 20;
        // flag to control PCM caching to avoid duplicate decoding
        bool enable_audio_pcm_caching = false;
        using DefaultAudioIOStrategy = engine::FileResourceLoader::DefaultAudioIOStrategy;
        DefaultAudioIOStrategy desktop_audio_io_strategy = DefaultAudioIOStrategy::Automatic;
        DefaultAudioIOStrategy wasm_audio_io_strategy = DefaultAudioIOStrategy::Automatic;
        // Which script to run when previewing an entity.
        QString preview_entity_script = "app://scripts/preview/entity.lua";
        // Which script to run when previewing a scene.
        QString preview_scene_script = "app://scripts/preview/scene.lua";
        // Which script to run when previewing a UI
        QString preview_ui_script = "app://scripts/preview/ui.lua";
    };

    void IntoJson(QJsonObject& json, const ProjectSettings& settings);
    void FromJson(const QJsonObject& json, ProjectSettings& settings);

} // app