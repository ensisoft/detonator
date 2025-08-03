cmake_minimum_required(VERSION 3.0)

project(Detonator-Shitscripten)
message(STATUS "
        ~~ DETONATOR 2D Emscripten ~~

    \\\\o Brought to you by Ensisoft o//
        http://www.ensisoft.com
    Copyright (c) 2020-2024 Sami Väisänen
                Ensisoft

    https://github.com/ensisoft/detonator
")

# https://cmake.org/cmake/help/latest/module/FetchContent.html#examples
# https://jonathanhamberg.com/post/cmake-embedding-git-hash/
include(FetchContent)
FetchContent_Declare(cmake_git_version_tracking
        GIT_REPOSITORY https://github.com/ensisoft/cmake-git-version-tracking.git
        GIT_TAG 7b0d0fcb7e6f5fcadd238dd1292350e9e1363d36
        )
FetchContent_MakeAvailable(cmake_git_version_tracking)

set(CMAKE_CXX_STANDARD 17)

include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../wdk")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/glm")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/box2d/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/libsndfile/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/libsamplerate/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/lua/src")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party "whatever")

add_link_options(-sMALLOC=mimalloc)
add_link_options(-sSTACK_SIZE=5MB)

# enable colored diagnostics
add_compile_options(-fcolor-diagnostics)
add_compile_options(-fansi-escape-codes)
add_compile_options(-fdiagnostics-color=always)

add_executable(UnitTest
    ../base/test_minimal.cpp
    ../base/unit_test/unit_test_math.cpp
    ../base/unit_test/unit_test.cpp
    ../base/unit_test/unit_test_grid.cpp
    ../base/unit_test/unit_test_memory.cpp
    ../audio/unit_test/unit_test_graph.cpp
    ../game/unit_test/unit_test_entity.cpp
    ../game/unit_test/unit_test_scene.cpp
    ../game/unit_test/unit_test_animation.cpp
    ../engine/unit_test/unit_test_lua.cpp
    ../audio/element.cpp
    ../audio/elements/delay.cpp
    ../audio/elements/effect.cpp
    ../audio/elements/file_source.cpp
    ../audio/elements/gain.cpp
    ../audio/elements/graph_class.cpp
    ../audio/elements/graph.cpp
    ../audio/elements/mixer.cpp
    ../audio/elements/mixer_source.cpp
    ../audio/elements/playlist.cpp
    ../audio/elements/queue.cpp
    ../audio/elements/resampler.cpp
    ../audio/elements/sine_source.cpp
    ../audio/elements/splitter.cpp
    ../audio/elements/stereo_joiner.cpp
    ../audio/elements/stereo_maker.cpp
    ../audio/elements/stereo_splitter.cpp
    ../audio/elements/stream_source.cpp
    ../audio/elements/zero_source.cpp
    ../audio/format.cpp
    ../audio/loader.cpp
    ../audio/mpg123.cpp
    ../audio/openal.cpp
    ../audio/player.cpp
    ../audio/sndfile.cpp
    ../audio/audio_file_source.cpp
    ../audio/audio_graph_source.cpp
    ../audio/thread_proxy_source.cpp
    ../base/assert.cpp
    ../base/format.cpp
    ../base/json.cpp
    ../base/logging.cpp
    ../base/threadpool.cpp
    ../base/trace.cpp
    ../base/utility.cpp
    ../data/io.cpp
    ../data/json.cpp
    ../graphics/material.cpp
    ../graphics/texture_map.cpp
    ../graphics/texture_texture_source.cpp
    ../graphics/texture_file_source.cpp
    ../graphics/texture_bitmap_buffer_source.cpp
    ../graphics/texture_bitmap_generator_source.cpp
    ../graphics/texture_text_buffer_source.cpp
    ../graphics/text_material.cpp
    ../graphics/material_class.cpp
    ../graphics/material_instance.cpp
    ../engine/audio.cpp
    ../engine/camera.cpp
    ../engine/context.cpp
    ../engine/lua_base.cpp
    ../engine/lua_data.cpp
    ../engine/lua_game.cpp
    ../engine/lua_game_runtime.cpp
    ../engine/lua_gfx.cpp
    ../engine/lua_glm.cpp
    ../engine/lua_uik.cpp
    ../engine/lua_util.cpp
    ../engine/lua_wdk.cpp
    ../engine/physics.cpp
    ../engine/state.cpp
    ../game/animator.cpp
    ../game/transform_animator.cpp
    ../game/kinematic_animator.cpp
    ../game/material_animator.cpp
    ../game/property_animator.cpp
    ../game/animation.cpp
    ../game/entity.cpp
    ../game/entity_node.cpp
    ../game/entity_placement.cpp
    ../game/entity_state.cpp
    ../game/entity_state_controller.cpp
    ../game/entity_node_rigid_body_joint.cpp
    ../game/entity_node_linear_mover.cpp
    ../game/entity_node_rigid_body.cpp
    ../game/entity_node_drawable_item.cpp
    ../game/entity_node_text_item.cpp
    ../game/entity_node_spatial_node.cpp
    ../game/entity_node_fixture.cpp
    ../game/entity_node_tilemap_node.cpp
    ../game/entity_node_light.cpp
    ../game/scene_class.cpp
    ../game/scene.cpp
    ../game/scriptvar.cpp
    ../game/tilemap.cpp
    ../game/tilemap_layer.cpp
    ../game/tilemap_layer_class.cpp
    ../uikit/animation.cpp
    ../uikit/widget.cpp
    ../uikit/window.cpp
        )
target_include_directories(UnitTest PRIVATE "${CMAKE_CURRENT_LIST_DIR}/test")
target_compile_definitions(UnitTest PRIVATE UNIT_TEST_BUNDLE)
target_compile_definitions(UnitTest PRIVATE DETONATOR_UNIT_TEST_WASM_BUILD)
target_compile_options(UnitTest PRIVATE -O3)
target_compile_options(UnitTest PRIVATE -fexceptions)
target_compile_options(UnitTest PRIVATE -fbracket-depth=1024)
target_compile_options(UnitTest PRIVATE -sUSE_BOOST_HEADERS)
target_compile_options(UnitTest PRIVATE -pthread)
target_link_options(UnitTest PRIVATE -sNO_DISABLE_EXCEPTION_CATCHING)
target_link_options(UnitTest PRIVATE -sWASM)
target_link_options(UnitTest PRIVATE -sALLOW_MEMORY_GROWTH)
target_link_options(UnitTest PRIVATE -sUSE_MPG123)
target_link_options(UnitTest PRIVATE -sUSE_BOOST_HEADERS)
target_link_options(UnitTest PRIVATE -pthread)
target_link_libraries(UnitTest sndfile)
target_link_libraries(UnitTest samplerate)
target_link_libraries(UnitTest Lua)
target_link_libraries(UnitTest box2d)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTest.js"   DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTest.wasm" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")


add_executable(UnitTestThread
        ../base/test_minimal.cpp
        ../base/unit_test/unit_test_thread.cpp
        ../audio/unit_test/unit_test.cpp
        ../base/trace.cpp
        ../base/logging.cpp
        ../base/assert.cpp
        ../base/json.cpp
        ../base/utility.cpp
        ../base/threadpool.cpp
        ../audio/loader.cpp
        ../audio/player.cpp
        ../audio/element.cpp
        ../audio/elements/delay.cpp
        ../audio/elements/effect.cpp
        ../audio/elements/file_source.cpp
        ../audio/elements/gain.cpp
        ../audio/elements/graph_class.cpp
        ../audio/elements/graph.cpp
        ../audio/elements/mixer.cpp
        ../audio/elements/mixer_source.cpp
        ../audio/elements/playlist.cpp
        ../audio/elements/queue.cpp
        ../audio/elements/resampler.cpp
        ../audio/elements/sine_source.cpp
        ../audio/elements/splitter.cpp
        ../audio/elements/stereo_joiner.cpp
        ../audio/elements/stereo_maker.cpp
        ../audio/elements/stereo_splitter.cpp
        ../audio/elements/stream_source.cpp
        ../audio/elements/zero_source.cpp
        ../audio/format.cpp
        ../audio/mpg123.cpp
        ../audio/sndfile.cpp
        ../audio/audio_file_source.cpp
        ../audio/thread_proxy_source.cpp
        ../audio/openal.cpp)

target_include_directories(UnitTestThread PRIVATE "${CMAKE_CURRENT_LIST_DIR}/test")
target_compile_definitions(UnitTestThread PRIVATE DETONATOR_UNIT_TEST_THREAD_WASM_BUILD)
target_compile_definitions(UnitTestThread PRIVATE UNIT_TEST_BUNDLE)
target_compile_definitions(UnitTestThread PRIVATE AUDIO_ENABLE_TEST_SOUND)
target_compile_options(UnitTestThread PRIVATE -pthread)
target_compile_options(UnitTestThread PRIVATE -O3)
target_compile_options(UnitTestThread PRIVATE -fexceptions)
target_compile_options(UnitTestThread PRIVATE -fbracket-depth=1024)
target_compile_options(UnitTestThread PRIVATE -sUSE_BOOST_HEADERS)
target_compile_options(UnitTestThread PRIVATE -sUSE_MPG123)
target_link_options(UnitTestThread PRIVATE -sNO_DISABLE_EXCEPTION_CATCHING)
target_link_options(UnitTestThread PRIVATE -sWASM)
target_link_options(UnitTestThread PRIVATE -sALLOW_MEMORY_GROWTH)
target_link_options(UnitTestThread PRIVATE -pthread)
target_link_options(UnitTestThread PRIVATE -sUSE_BOOST_HEADERS)
target_link_options(UnitTestThread PRIVATE -sUSE_MPG123)

# Shitscripten shitting itself on string decode
# https://github.com/emscripten-core/emscripten/issues/18034
target_link_options(UnitTestThread PRIVATE -sTEXTDECODER=0)

# You can't just spawn a thread in WASM but the thread (web worker)
# creation must happen from the main thread's event loop (or something)
# So if the main thread creates a thread and waits/joins it without ever
# returning to the browser the application will hang.
#
# Using the PTHREAD_POOL_SIZE=x can create some web workers at application
# start so that the pthread_create (std::thread create) can pick a
# thread from the pool without having to return to browser's event loop first.
#
target_link_options(UnitTestThread PRIVATE -sPTHREAD_POOL_SIZE=10)
target_link_options(UnitTestThread PRIVATE -sUSE_MPG123)
target_link_options(UnitTestThread PRIVATE -pthread)
target_link_libraries(UnitTestThread sndfile)
target_link_libraries(UnitTestThread samplerate)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTestThread.js"        DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTestThread.worker.js" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTestThread.wasm"      DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")

set(ENGINE_SRC
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/element.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/delay.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/effect.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/file_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/gain.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/graph_class.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/graph.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/mixer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/mixer_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/playlist.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/queue.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/resampler.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/sine_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/splitter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/stereo_joiner.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/stereo_maker.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/stereo_splitter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/stream_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/elements/zero_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/format.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/loader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/mpg123.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/openal.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/player.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/sdl.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/sndfile.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/sokol.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/audio_file_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/audio_graph_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../audio/thread_proxy_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/assert.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/format.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/json.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/logging.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/logging.h
  ${CMAKE_CURRENT_LIST_DIR}/../base/threadpool.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/trace.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../base/utility.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../data/json.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../data/io.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/animation.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/animator.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_placement.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_drawable_item.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_fixture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_light.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_rigid_body.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_rigid_body_joint.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_spatial_node.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_text_item.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_tilemap_node.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_node_linear_mover.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_state.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/entity_state_controller.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/kinematic_animator.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/material_animator.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/property_animator.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/scene_class.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/scene.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/scriptvar.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/tilemap.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/tilemap_layer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/tilemap_layer_class.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../game/transform_animator.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/audio.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/camera.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/cpp_game_runtime.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/engine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/engine-logo.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/graphics.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/loader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_base.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_data.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_game.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_game_runtime.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_gfx.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_glm.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_uik.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_util.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/lua_wdk.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/physics.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/renderer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/state.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../engine/ui.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../device/opengles.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../device/vertex.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/algo.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/bitmap.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/bitmap_noise.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/debug_drawable.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device_framebuffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device_geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device_instance.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device_program.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device_shader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/device_texture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/drawable.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/drawcmd.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/drawing.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/generic_shader_program.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/guidegrid.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/image.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/linebatch.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/loader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/material.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/material_class.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/material_instance.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/painter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/particle_engine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/pixel.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/polygon_mesh.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/shader_program.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/shader_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/simple_shape.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/text_buffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/text_font.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/text_material.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/texture_bitmap_buffer_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/texture_bitmap_generator_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/texture_file_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/texture_map.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/texture_text_buffer_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/texture_texture_source.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/tilebatch.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/utility.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../graphics/vertex.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../wdk/wdk/keys.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../uikit/animation.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../uikit/window.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../uikit/widget.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../third_party/base64/base64.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../third_party/stb/stb_image_write.c
  ${CMAKE_CURRENT_LIST_DIR}/../third_party/stb/stb_image.c)

# engine include directories.
set(ENGINE_INC "${CMAKE_CURRENT_LIST_DIR}/../config/")

set(ENGINE_DEF
    DETONATOR_GAME_ENGINE_WASM_BUILD
    DETONATOR_ENGINE_BUILD)

set(ENGINE_LIBS
    cmake_git_version_tracking
    box2d
    sndfile
    samplerate
    Lua)

# NOTE: the flags that are switched on "-s" can be both linker or compile flags
set(ENGINE_COMPILE_OPTIONS
    -fbracket-depth=1024
    -fexceptions
    -O3
    -pthread
    -sUSE_MPG123
    -sUSE_HARFBUZZ
    -sUSE_FREETYPE
    -sUSE_BOOST_HEADERS)

# disabled for now since this seems to be broken somehow
# and creates distortion in the sound.
#list(APPEND ENGINE_COMPILE_OPTIONS -msse2)
#list(APPEND ENGINE_COMPILE_OPTIONS -msimd128)

# SDL is needed for the SDL audio backend but we're not using
# that right now so this can be disabled.
#list(APPEND ENGINE_COMPILE_OPTIONS -sUSE_SDL=2)

set(ENGINE_LINK_OPTIONS
    -sNO_DISABLE_EXCEPTION_CATCHING
    -sUSE_MPG123
    -sUSE_HARFBUZZ
    -sUSE_FREETYPE
    -sUSE_BOOST_HEADERS
    -sFORCE_FILESYSTEM
    -sWASM
    -sALLOW_MEMORY_GROWTH
    -lidbfs.js
    -sEXPORTED_FUNCTIONS=_main,_gui_set_flag
    -sEXPORTED_RUNTIME_METHODS=ccall
    -sUSE_WEBGL2
    -sFULL_ES3
    -pthread
    -sMALLOC=mimalloc
    -sSTACK_SIZE=5MB)

# Shitscripten shitting itself on string decode
# https://github.com/emscripten-core/emscripten/issues/18034
list(APPEND ENGINE_LINK_OPTIONS -sTEXTDECODER=0)

# SDL is needed for the SDL audio backend but we're not using
# that right now so this can be disabled.
#list(APPEND ENGINE_LINK_OPTIONS -sUSE_SDL=2)

# See the unit test comments about thread pool creation
# and the limitations.
# the number of threads here needs to match the code's
# expectation of threads!
list(APPEND ENGINE_LINK_OPTIONS -sPTHREAD_POOL_SIZE=4)

#### BUILD DEFAULT GAME ENGINE
add_executable(GameEngine ${ENGINE_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/../engine/game_dummy.cpp)

target_include_directories(GameEngine PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../config/")
target_compile_definitions(GameEngine PRIVATE ${ENGINE_DEF})
target_compile_options(GameEngine PRIVATE ${ENGINE_COMPILE_OPTIONS})
target_link_options(GameEngine PRIVATE ${ENGINE_LINK_OPTIONS})
target_link_libraries(GameEngine ${ENGINE_LIBS})

# emscripten cache (when emscripten is installed from github)
# seems to be under /home/user/emsdk/upstream/emscripten/cache

set(ENGINE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/_deps/cmake_git_version_tracking-build/git.c")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.js"        DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.wasm"      DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.worker.js" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5")
install(FILES "${ENGINE_VERSION_FILE}"                           DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5" RENAME "GameEngine.version.txt")

# build custom game engines.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../editor/dist/demos/blast/cpp "blast")
