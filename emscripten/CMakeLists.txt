cmake_minimum_required(VERSION 3.0)

project(GameStudio-Emscripten)
message(STATUS "
     ~~ Ensisoft GameStudio Emscripten ~~

    \\\\o Brought to you by Ensisoft o//
        http://www.ensisoft.com
    Copyright (c) 2021 Sami Väisänen
                Ensisoft

https://github.com/ensisoft/gamestudio
")

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

add_executable(UnitTest
    ../base/test_minimal.cpp
    ../base/unit_test/unit_test_math.cpp
    ../base/unit_test/unit_test.cpp
    ../base/unit_test/unit_test_grid.cpp
    ../base/unit_test/unit_test_memory.cpp
    ../game/unit_test/unit_test_entity.cpp
    ../game/unit_test/unit_test_scene.cpp
    ../game/unit_test/unit_test_animation.cpp
    ../base/trace.cpp
    ../base/logging.cpp
    ../base/assert.cpp
    ../base/json.cpp
    ../base/utility.cpp
    ../data/json.cpp
    ../game/scriptvar.cpp
    ../game/animation.cpp
    ../game/actuator.cpp
    ../game/entity.cpp
    ../game/scene.cpp)
target_compile_definitions(UnitTest PRIVATE UNIT_TEST_BUNDLE)
target_compile_options(UnitTest PRIVATE -O3)
target_compile_options(UnitTest PRIVATE -fexceptions)
target_compile_options(UnitTest PRIVATE -fbracket-depth=1024)
target_link_options(UnitTest PRIVATE -sWASM)
target_link_options(UnitTest PRIVATE -sALLOW_MEMORY_GROWTH)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTest.js"   DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UnitTest.wasm" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin")

add_executable(GameEngine
    main.cpp
    ../audio/loader.cpp
    ../audio/player.cpp
    ../audio/element.cpp
    ../audio/format.cpp
    ../audio/graph.cpp
    ../audio/mpg123.cpp
    ../audio/sndfile.cpp
    ../audio/openal.cpp
    ../audio/source.cpp
    ../base/assert.cpp
    ../base/logging.h
    ../base/logging.cpp
    ../base/utility.cpp
    ../base/format.cpp
    ../base/trace.cpp
    ../base/json.cpp
    ../data/json.cpp
    ../data/io.cpp
    ../game/animation.cpp
    ../game/actuator.cpp
    ../game/entity.cpp
    ../game/scene.cpp
    ../game/scriptvar.cpp
    ../game/tilemap.cpp
    ../engine/camera.cpp
    ../engine/audio.cpp
    ../engine/engine.cpp
    ../engine/loader.cpp
    ../engine/lua_game_runtime.cpp
    ../engine/lua_util.cpp
    ../engine/lua_base.cpp
    ../engine/lua_data.cpp
    ../engine/lua_glm.cpp
    ../engine/lua_wdk.cpp
    ../engine/lua_uik.cpp
    ../engine/lua_gfx.cpp
    ../engine/lua_game.cpp
    ../engine/physics.cpp
    ../engine/renderer.cpp
    ../engine/state.cpp
    ../engine/ui.cpp
    ../engine/graphics.cpp
    ../device/opengles.cpp
    ../graphics/algo.cpp
    ../graphics/shaderpass.cpp
    ../graphics/painter.cpp
    ../graphics/material.cpp
    ../graphics/drawable.cpp
    ../graphics/image.cpp
    ../graphics/loader.cpp
    ../graphics/drawing.cpp
    ../graphics/bitmap.cpp
    ../graphics/text.cpp
    ../graphics/utility.cpp
    ../wdk/wdk/keys.cpp
    ../uikit/window.cpp
    ../uikit/widget.cpp
    ../third_party/base64/base64.cpp
    ../third_party/stb/stb_image_write.c
    ../third_party/stb/stb_image.c
    )
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

# emscripten cache (when emscripten is installed from github)
# seems to be under /home/user/emsdk/upstream/emscripten/cache

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.js"   DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.wasm" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist")
#install(FILES   ${CMAKE_CURRENT_BINARY_DIR}/test-engine.worker.js DESTINATION "${CMAKE_CURRENT_LIST_DIR}/dist")
