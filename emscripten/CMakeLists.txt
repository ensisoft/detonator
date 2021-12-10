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

add_executable(GameEngine
    main.cpp
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
    ../game/animation.cpp
    ../game/entity.cpp
    ../game/scene.cpp
    ../game/scriptvar.cpp
    ../engine/audio.cpp
    ../engine/engine.cpp
    ../engine/loader.cpp
    ../engine/lua.cpp
    ../engine/physics.cpp
    ../engine/renderer.cpp
    ../engine/state.cpp
    ../engine/ui.cpp
    ../graphics/painter.cpp
    ../graphics/material.cpp
    ../graphics/drawable.cpp
    ../graphics/image.cpp
    ../graphics/loader.cpp
    ../graphics/drawing.cpp
    ../graphics/bitmap.cpp
    ../graphics/text.cpp
    ../graphics/opengl_es2_device.cpp
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

# okay so these work like these (the CMake way)
target_compile_options(GameEngine PRIVATE -fbracket-depth=1024 -fexceptions)
target_compile_options(GameEngine PRIVATE -O3)
target_link_options(GameEngine PRIVATE -lidbfs.js)
# disabled for now since this seems to be broken somehow
# and creates distortion in the sound.
#target_compile_options(test-engine PRIVATE -msse2 -msimd128)

# emscripten is "special".
# the flags that are switched on "-s" can be both linker or compile flags
# and they don't work with target_compile_options or with target_link_options

set_target_properties(GameEngine PROPERTIES COMPILE_FLAGS "-s USE_MPG123=1 -s USE_HARFBUZZ=1 -s USE_FREETYPE=1 -s USE_BOOST_HEADERS=1")

set_target_properties(GameEngine PROPERTIES LINK_FLAGS "-s NO_DISABLE_EXCEPTION_CATCHING -s USE_MPG123=1 -s USE_HARFBUZZ=1 -s USE_FREETYPE=1 -s USE_BOOST_HEADERS=1 -s FORCE_FILESYSTEM=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 ")
#target_compile_options(test PRIVATE -fbracket-depth=1024 -fexceptions)
#target_compile_options(test PRIVATE -s FORCE_FILESYSTEM=1)
#target_compile_options(test PRIVATE -s USE_HARFBUZZ=1)
#target_compile_options(test PRIVATE -s USE_FREETYPE=1)
#target_compile_options(test PRIVATE -s USE_BOOST_HEADERS=1)

#target_link_options(test PRIVATE -s FORCE_FILESYSTEM=1)
#target_link_options(test PRIVATE -s USE_HARFBUZZ=1)
#target_link_options(test PRIVATE -s USE_FREETYPE=1)
#target_link_options(test PRIVATE -s USE_BOOST_HEADERS=1)

# emscripten cache (when emscripten is installed from github)
# seems to be under /home/user/emsdk/upstream/emscripten/cache

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.js"   DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.wasm" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist")
#install(FILES   ${CMAKE_CURRENT_BINARY_DIR}/test-engine.worker.js DESTINATION "${CMAKE_CURRENT_LIST_DIR}/dist")
