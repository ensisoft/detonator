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
    ../audio/graph.cpp
    ../audio/loader.cpp
    ../audio/mpg123.cpp
    ../audio/openal.cpp
    ../audio/player.cpp
    ../audio/proxy.cpp
    ../audio/sndfile.cpp
    ../audio/source.cpp
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
    ../game/entity_node_transformer.cpp
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
    ../uikit/animation.cpp
    ../uikit/widget.cpp
    ../uikit/window.cpp
        )
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
        ../audio/source.cpp
        ../audio/proxy.cpp
        ../audio/openal.cpp)

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


add_executable(GameEngine
    main.cpp
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
    ../audio/graph.cpp
    ../audio/loader.cpp
    ../audio/mpg123.cpp
    ../audio/openal.cpp
    ../audio/player.cpp
    ../audio/proxy.cpp
    ../audio/sdl.cpp
    ../audio/sndfile.cpp
    ../audio/sokol.cpp
    ../audio/source.cpp
    ../base/assert.cpp
    ../base/format.cpp
    ../base/json.cpp
    ../base/logging.cpp
    ../base/logging.h
    ../base/threadpool.cpp
    ../base/trace.cpp
    ../base/utility.cpp
    ../data/json.cpp
    ../data/io.cpp
    ../game/animation.cpp
    ../game/animator.cpp
    ../game/entity.cpp
    ../game/entity_node.cpp
    ../game/entity_placement.cpp
    ../game/entity_node_drawable_item.cpp
    ../game/entity_node_fixture.cpp
    ../game/entity_node_light.cpp
    ../game/entity_node_rigid_body.cpp
    ../game/entity_node_rigid_body_joint.cpp
    ../game/entity_node_spatial_node.cpp
    ../game/entity_node_text_item.cpp
    ../game/entity_node_tilemap_node.cpp
    ../game/entity_node_transformer.cpp
    ../game/entity_state.cpp
    ../game/entity_state_controller.cpp
    ../game/kinematic_animator.cpp
    ../game/material_animator.cpp
    ../game/property_animator.cpp
    ../game/scene_class.cpp
    ../game/scene.cpp
    ../game/scriptvar.cpp
    ../game/tilemap.cpp
    ../game/transform_animator.cpp
    ../engine/audio.cpp
    ../engine/camera.cpp
    ../engine/engine.cpp
    ../engine/graphics.cpp
    ../engine/loader.cpp
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
    ../engine/renderer.cpp
    ../engine/state.cpp
    ../engine/ui.cpp
    ../device/opengles.cpp
    ../device/vertex.cpp
    ../graphics/algo.cpp
    ../graphics/bitmap.cpp
    ../graphics/bitmap_noise.cpp
    ../graphics/debug_drawable.cpp
    ../graphics/device.cpp
    ../graphics/device_framebuffer.cpp
    ../graphics/device_geometry.cpp
    ../graphics/device_instance.cpp
    ../graphics/device_program.cpp
    ../graphics/device_shader.cpp
    ../graphics/device_texture.cpp
    ../graphics/drawable.cpp
    ../graphics/drawcmd.cpp
    ../graphics/drawing.cpp
    ../graphics/generic_shader_program.cpp
    ../graphics/geometry.cpp
    ../graphics/guidegrid.cpp
    ../graphics/image.cpp
    ../graphics/linebatch.cpp
    ../graphics/loader.cpp
    ../graphics/material.cpp
    ../graphics/material_class.cpp
    ../graphics/material_instance.cpp
    ../graphics/painter.cpp
    ../graphics/particle_engine.cpp
    ../graphics/pixel.cpp
    ../graphics/polygon_mesh.cpp
    ../graphics/shader_program.cpp
    ../graphics/shader_source.cpp
    ../graphics/simple_shape.cpp
    ../graphics/text_buffer.cpp
    ../graphics/text_font.cpp
    ../graphics/text_material.cpp
    ../graphics/texture_bitmap_buffer_source.cpp
    ../graphics/texture_bitmap_generator_source.cpp
    ../graphics/texture_file_source.cpp
    ../graphics/texture_map.cpp
    ../graphics/texture_text_buffer_source.cpp
    ../graphics/texture_texture_source.cpp
    ../graphics/tilebatch.cpp
    ../graphics/utility.cpp
    ../graphics/vertex.cpp
    ../wdk/wdk/keys.cpp
    ../uikit/animation.cpp
    ../uikit/window.cpp
    ../uikit/widget.cpp
    ../third_party/base64/base64.cpp
    ../third_party/stb/stb_image_write.c
    ../third_party/stb/stb_image.c
    )

target_compile_definitions(GameEngine PRIVATE DETONATOR_GAME_ENGINE_WASM_BUILD)

target_link_libraries(GameEngine cmake_git_version_tracking)

target_link_libraries(GameEngine box2d)
target_link_libraries(GameEngine sndfile)
target_link_libraries(GameEngine samplerate)
target_link_libraries(GameEngine Lua)

target_compile_options(GameEngine PRIVATE -fbracket-depth=1024 -fexceptions)
target_compile_options(GameEngine PRIVATE -O3)
# disabled for now since this seems to be broken somehow
# and creates distortion in the sound.
#target_compile_options(test-engine PRIVATE -msse2 -msimd128)

# the flags that are switched on "-s" can be both linker or compile flags
target_compile_options(GameEngine PRIVATE -sUSE_MPG123)
target_compile_options(GameEngine PRIVATE -sUSE_HARFBUZZ)
target_compile_options(GameEngine PRIVATE -sUSE_FREETYPE)
target_compile_options(GameEngine PRIVATE -sUSE_BOOST_HEADERS)
# SDL is needed for the SDL audio backend
#target_compile_options(GameEngine PRIVATE -sUSE_SDL=2)

# uncomment this in order to enable threading
target_compile_options(GameEngine PRIVATE -pthread)

target_link_options(GameEngine PRIVATE -sNO_DISABLE_EXCEPTION_CATCHING)
target_link_options(GameEngine PRIVATE -sUSE_MPG123)
target_link_options(GameEngine PRIVATE -sUSE_HARFBUZZ)
target_link_options(GameEngine PRIVATE -sUSE_FREETYPE)
target_link_options(GameEngine PRIVATE -sUSE_BOOST_HEADERS)
target_link_options(GameEngine PRIVATE -sFORCE_FILESYSTEM)
target_link_options(GameEngine PRIVATE -sWASM)
target_link_options(GameEngine PRIVATE -sALLOW_MEMORY_GROWTH)
target_link_options(GameEngine PRIVATE -lidbfs.js)
target_link_options(GameEngine PRIVATE -sEXPORTED_FUNCTIONS=_main,_gui_set_flag)
target_link_options(GameEngine PRIVATE -sEXPORTED_RUNTIME_METHODS=ccall)
target_link_options(GameEngine PRIVATE -sUSE_WEBGL2)
target_link_options(GameEngine PRIVATE -sFULL_ES3)

# Shitscripten shitting itself on string decode
# https://github.com/emscripten-core/emscripten/issues/18034
target_link_options(GameEngine PRIVATE -sTEXTDECODER=0)

# SDL is needed for the SDL audio backend
#target_link_options(GameEngine PRIVATE -sUSE_SDL=2)

# uncomment this in order to enable threading
target_link_options(GameEngine PRIVATE -pthread)
# See the unit test comments about thread pool creation
# and the limitations.
# the number of threads here needs to match the code's
# expectation of threads!
target_link_options(GameEngine PRIVATE -sPTHREAD_POOL_SIZE=4)

# emscripten cache (when emscripten is installed from github)
# seems to be under /home/user/emsdk/upstream/emscripten/cache

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.js"   DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.wasm" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5")

# uncomment this in order to enable threading
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.worker.js" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/_deps/cmake_git_version_tracking-build/git.c"
        DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/../editor/dist/html5"
        RENAME "GameEngineVersion.txt")


