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

#include_directories(BEFORE "/usr/lib/emscripten/system/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../wdk")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/glm")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/box2d/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/../third_party/box2d/src")

# emcc --show-ports
set(BOX2D_SOURCE_FILES
    ../third_party/box2d/src/collision/b2_broad_phase.cpp
    ../third_party/box2d/src/collision/b2_chain_shape.cpp
    ../third_party/box2d/src/collision/b2_circle_shape.cpp
    ../third_party/box2d/src/collision/b2_collide_circle.cpp
    ../third_party/box2d/src/collision/b2_collide_edge.cpp
    ../third_party/box2d/src/collision/b2_collide_polygon.cpp
    ../third_party/box2d/src/collision/b2_collision.cpp
    ../third_party/box2d/src/collision/b2_distance.cpp
    ../third_party/box2d/src/collision/b2_dynamic_tree.cpp
    ../third_party/box2d/src/collision/b2_edge_shape.cpp
    ../third_party/box2d/src/collision/b2_polygon_shape.cpp
    ../third_party/box2d/src/collision/b2_time_of_impact.cpp
    ../third_party/box2d/src/common/b2_block_allocator.cpp
    ../third_party/box2d/src/common/b2_draw.cpp
    ../third_party/box2d/src/common/b2_math.cpp
    ../third_party/box2d/src/common/b2_settings.cpp
    ../third_party/box2d/src/common/b2_stack_allocator.cpp
    ../third_party/box2d/src/common/b2_timer.cpp
    ../third_party/box2d/src/dynamics/b2_body.cpp
    ../third_party/box2d/src/dynamics/b2_chain_circle_contact.cpp
    ../third_party/box2d/src/dynamics/b2_chain_polygon_contact.cpp
    ../third_party/box2d/src/dynamics/b2_circle_contact.cpp
    ../third_party/box2d/src/dynamics/b2_contact.cpp
    ../third_party/box2d/src/dynamics/b2_contact_manager.cpp
    ../third_party/box2d/src/dynamics/b2_contact_solver.cpp
    ../third_party/box2d/src/dynamics/b2_distance_joint.cpp
    ../third_party/box2d/src/dynamics/b2_edge_circle_contact.cpp
    ../third_party/box2d/src/dynamics/b2_edge_polygon_contact.cpp
    ../third_party/box2d/src/dynamics/b2_fixture.cpp
    ../third_party/box2d/src/dynamics/b2_friction_joint.cpp
    ../third_party/box2d/src/dynamics/b2_gear_joint.cpp
    ../third_party/box2d/src/dynamics/b2_island.cpp
    ../third_party/box2d/src/dynamics/b2_joint.cpp
    ../third_party/box2d/src/dynamics/b2_motor_joint.cpp
    ../third_party/box2d/src/dynamics/b2_mouse_joint.cpp
    ../third_party/box2d/src/dynamics/b2_polygon_circle_contact.cpp
    ../third_party/box2d/src/dynamics/b2_polygon_contact.cpp
    ../third_party/box2d/src/dynamics/b2_prismatic_joint.cpp
    ../third_party/box2d/src/dynamics/b2_pulley_joint.cpp
    ../third_party/box2d/src/dynamics/b2_revolute_joint.cpp
    ../third_party/box2d/src/dynamics/b2_weld_joint.cpp
    ../third_party/box2d/src/dynamics/b2_wheel_joint.cpp
    ../third_party/box2d/src/dynamics/b2_world.cpp
    ../third_party/box2d/src/dynamics/b2_world_callbacks.cpp
    ../third_party/box2d/src/rope/b2_rope.cpp)


add_executable(test-engine
    main.cpp
    ../base/assert.cpp
    ../base/logging.h
    ../base/logging.cpp
    ../base/utility.cpp
    ../base/format.cpp
    ../base/trace.cpp
    ../base/json.cpp
    ../game/animation.cpp
    ../game/entity.cpp
    ../game/scene.cpp
    ../game/scriptvar.cpp
    ../engine/test/main.cpp
    ../engine/renderer.cpp
    ../engine/ui.cpp
    ../engine/physics.cpp
    ../graphics/painter.cpp
    ../graphics/material.cpp
    ../graphics/drawable.cpp
    ../graphics/image.cpp
    ../graphics/loader.cpp
    ../graphics/drawing.cpp
    ../graphics/bitmap.cpp
    ../graphics/text.cpp
    ../graphics/opengl_es2_device.cpp
    ../uikit/window.cpp
    ../uikit/widget.cpp
    ../third_party/base64/base64.cpp
    ../third_party/stb/stb_image_write.c
    ../third_party/stb/stb_image.c
    ${BOX2D_SOURCE_FILES}
    )

# okay so these work like these (the CMake way)
target_compile_options(test-engine PRIVATE -fbracket-depth=1024 -fexceptions)
target_compile_options(test-engine PRIVATE -O3)

# emscripten is "special".
# the flags that are switched on "-s" can be both linker or compile flags
# and they don't work with target_compile_options or with target_link_options

set_target_properties(test-engine PROPERTIES COMPILE_FLAGS "-s USE_HARFBUZZ=1 -s USE_FREETYPE=1 -s USE_BOOST_HEADERS=1")

set_target_properties(test-engine PROPERTIES LINK_FLAGS "-s USE_HARFBUZZ=1 -s USE_FREETYPE=1 -s USE_BOOST_HEADERS=1 -s FORCE_FILESYSTEM=1 -s WASM=1")
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

install(TARGETS test-engine                                  DESTINATION "${CMAKE_CURRENT_LIST_DIR}/dist")
install(FILES   ${CMAKE_CURRENT_BINARY_DIR}/test-engine.wasm DESTINATION "${CMAKE_CURRENT_LIST_DIR}/dist")