cmake_minimum_required(VERSION 3.0)

project(Gamestudio)

message(STATUS "
          ~~ Ensisoft GameStudio ~~

    \\\\o Brought to you by Ensisoft o//
        http://www.ensisoft.com
    Copyright (c) 2019 Sami Väisänen
                Ensisoft

https://github.com/ensisoft/gamestudio

")

# begin with the turds in 3rd party libs

# Freetype2
option(WITH_ZLIB "Freetype: Enable zlib " OFF)
option(WITH_BZip2 "Freetype: Enable bzip2 " OFF)
option(WITH_HarfBuzz "Freetype: Enable harfbuzz" OFF)
option(WITH_PNG "Freetype: Build with PNG" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype)

set_property(TARGET freetype_custom_build PROPERTY POSITION_INDEPENDENT_CODE ON)

# harfbuzz (depends on freetype2)
set(HARFBUZZ_FREETYPE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype/include" CACHE STRING "Freetype2 include folder")
set(HARFBUZZ_FREETYPE_LIBRARY "freetype_custom_build" CACHE STRING "Name of the Freetype2 library build.")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz)

set_property(TARGET harfbuzz_custom_build PROPERTY POSITION_INDEPENDENT_CODE ON)

# Qt color widgets
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt-Color-Widgets)

# wdk
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/wdk)

# Unfortunately we need boost...
set(Boost_USE_STATIC_LIBS ON)
if(WIN32)
    set(BOOST_ROOT $ENV{BOOST_ROOT})
    if (NOT BOOST_ROOT)
        set(BOOST_ROOT "c:/local/boost_1_72_0")
        message("
Bollocks. BOOST_ROOT was not set. Defaulting to ${BOOST_ROOT}
TL;DR BOOST_ROOT is a variable that points to your Boost installation
root. So for example if your Boost is installed in C:/boost_1_55_0
your BOOST_ROOT should be c:/boost_1_55_0.
You can set the variable in M$ command prompt with

        $ set BOOST_ROOT=c:/boost_1_55_0

and in Linux in your terminal (in bash)

        $ export BOOST_ROOT=/usr/local/boost_1_55_0

")
    endif()
endif()

# for some reason the find_package fails on windows when we say which
# libs we'd like to use. Anyway the linking is automatic so we'll just
# add the libs manually to linking options for Linux.
#find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Boost REQUIRED COMPONENTS)
if(NOT Boost_FOUND)
    if (UNIX)
        message(FATAL_ERROR "
Boost (http://www.boost.org) development libraries were not found.
You should be able to use the boost dev packages available in your distro's repos.
On Ubuntu you might want to try 'sudo apt-get install libboost-all-dev'
On other distros please consult your distro's docs on how to install Boost.
I'm going to crap out now. >.<
")
    elseif(WIN32)
        message(FATAL_ERROR "
Boost (http://www.boost.org) development libraries were not found.
You can download a prebuilt binary package from http://www.boost.org.
Or if you're using Visual Studio 2015 you can follow this direct link:
https://sourceforge.net/projects/boost/files/boost-binaries/1.61.0/boost_1_61_0-msvc-14.0-64.exe/download
If you have boost but not in a default location or another version of boost
you can try to specify BOOST_ROOT environment variable.
I'm going to crap out now. >.<
")
    endif()
endif()

# why is this not set!?
# should come from cmake through find_package(Boost, ...)
if (MSVC)
    if(NOT Boost_LIBRARY_DIRS)
        message(WARNING "
Boost_LIBRARY_DIRS is not set.
I'm going to use a default for MSVS 14 2015 Win64.
If that doesn't match your build environment/compiler/arch you will need
to edit the line below this message manually.
Sorry about that :-(
")
        set(Boost_LIBRARY_DIRS "${BOOST_ROOT}/lib64-msvc-14.0")
    endif()
endif()

message(STATUS "${Boost_INCLUDE_DIRS}")
message(STATUS "${Boost_LIBRARY_DIRS}")

include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/wdk")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/glm")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/freetype/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/harfbuzz/src")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/harfbuzz/src/generated")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/Khronos")
include_directories(BEFORE "${Boost_INCLUDE_DIRS}")
link_directories("${Boost_LIBRARY_DIRS}")

# see this bug report about C++14 and Qt
# https://bugreports.qt.io/browse/QTBUG-53002
set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    # /wd4251 Qt dll interface crap
    # /wd4275 Qt dll interface crap again
    # /wd4018 signed/unsigned mismatch
    # /wd4244 conversion from double to float
    # /wd4305 truncation from double to float
    # /wd4267 conversion from size_t to int (Qt uses ints extensively)
    add_definitions("/wd4251 /wd4275 /wd4018 /wd4244 /wd4305 /wd4267")
endif()

# we also need Qt for the time being
if (WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/5.12.6/msvc2017_64")
endif()

find_package(Qt5Widgets)
find_package(Qt5OpenGL)
find_package(Qt5Network)
if (NOT Qt5Widgets_FOUND)
    message("
Qt5 was not found. I will not be able to build the game.
")
else()
    message(STATUS "Qt5 version: ${Qt5Widgets_VERSION}")
    message(STATUS "${Qt5Widgets_INCLUDE_DIRS}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
# this tells cmake to include our *current* directory which is our build directory
# in which the Qt tools (moc and uic) will spit out their generated files.
# so for example when you have "include ui_foobar.h" in your code the
# ui_foobar.h is generated by UIC and will be placed in the current dir.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Editor application.
add_executable(Editor
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    graphics/drawing.cpp
    graphics/drawable.cpp
    graphics/material.h
    graphics/material.cpp
    graphics/image.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/resource.cpp
    gamelib/animation.cpp
    editor/app/event.h
    editor/app/eventlog.h
    editor/app/eventlog.cpp
    editor/app/format.h
    editor/app/format.cpp
    editor/app/packing.h
    editor/app/packing.cpp
    editor/app/utility.h
    editor/app/utility.cpp
    editor/app/workspace.h
    editor/app/workspace.cpp
    editor/app/packing.cpp
    editor/app/process.h
    editor/app/process.cpp
    editor/app/ipc.cpp
    editor/gui/animationwidget.h
    editor/gui/animationwidget.ui
    editor/gui/animationwidget.cpp
    editor/gui/animationtrackwidget.h
    editor/gui/animationtrackwidget.ui
    editor/gui/animationtrackwidget.cpp
    editor/gui/childwindow.h
    editor/gui/childwindow.cpp
    editor/gui/childwindow.ui
    editor/gui/playwindow.h
    editor/gui/playwindow.cpp
    editor/gui/playwindow.ui
    editor/gui/dlgsettings.h
    editor/gui/dlgsettings.ui
    editor/gui/dlgsettings.cpp
    editor/gui/dlgbitmap.h
    editor/gui/dlgbitmap.cpp
    editor/gui/dlgtext.h
    editor/gui/dlgtext.cpp
    editor/gui/dlgtext.ui
    editor/gui/dlgtexturerect.h
    editor/gui/dlgtexturerect.ui
    editor/gui/dlgtexturerect.cpp
    editor/gui/dlgimgpack.ui
    editor/gui/dlgimgpack.h
    editor/gui/dlgimgpack.cpp
    editor/gui/dlgpackage.h
    editor/gui/dlgpackage.ui
    editor/gui/dlgpackage.cpp
    editor/gui/dlgproject.h
    editor/gui/dlgproject.ui
    editor/gui/dlgproject.cpp
    editor/gui/dlgopen.h
    editor/gui/dlgopen.ui
    editor/gui/dlgopen.cpp
    editor/gui/dlgnew.h
    editor/gui/dlgnew.ui
    editor/gui/gfxwidget.h
    editor/gui/gfxwidget.cpp
    editor/gui/mainwidget.h
    editor/gui/mainwindow.h
    editor/gui/mainwindow.cpp
    editor/gui/mainwindow.ui
    editor/gui/main.cpp
    editor/gui/particlewidget.h
    editor/gui/particlewidget.cpp
    editor/gui/particlewidget.ui
    editor/gui/polygonwidget.ui
    editor/gui/polygonwidget.h
    editor/gui/polygonwidget.cpp
    editor/gui/materialwidget.h
    editor/gui/materialwidget.cpp
    editor/gui/materialwidget.ui
    editor/gui/settings.h
    editor/gui/settings.cpp
    editor/gui/treewidget.h
    editor/gui/treewidget.cpp
    editor/gui/timelinewidget.h
    editor/gui/timelinewidget.cpp
    editor/resource/resource.qrc
    editor/resource/qdarkstyle/style.qrc
    third_party/stb/stb_image.c
    third_party/base64/base64.cpp
    third_party/Qt-Color-Widgets/src/abstract_widget_list.cpp
    third_party/Qt-Color-Widgets/src/bound_color_selector.cpp
    third_party/Qt-Color-Widgets/src/color_2d_slider.cpp
    third_party/Qt-Color-Widgets/src/color_delegate.cpp
    third_party/Qt-Color-Widgets/src/color_dialog.cpp
    third_party/Qt-Color-Widgets/src/color_dialog.ui
    third_party/Qt-Color-Widgets/src/color_line_edit.cpp
    third_party/Qt-Color-Widgets/src/color_list_widget.cpp
    third_party/Qt-Color-Widgets/src/color_names.cpp
    third_party/Qt-Color-Widgets/src/color_palette.cpp
    third_party/Qt-Color-Widgets/src/color_palette_model.cpp
    third_party/Qt-Color-Widgets/src/color_palette_widget.cpp
    third_party/Qt-Color-Widgets/src/color_palette_widget.ui
    third_party/Qt-Color-Widgets/src/color_preview.cpp
    third_party/Qt-Color-Widgets/src/color_selector.cpp
    third_party/Qt-Color-Widgets/src/color_utils.cpp
    third_party/Qt-Color-Widgets/src/color_wheel.cpp
    third_party/Qt-Color-Widgets/src/color_widgets.qrc
    third_party/Qt-Color-Widgets/src/gradient_slider.cpp
    third_party/Qt-Color-Widgets/src/hue_slider.cpp
    third_party/Qt-Color-Widgets/src/swatch.cpp
    third_party/Qt-Color-Widgets/include/abstract_widget_list.hpp
    third_party/Qt-Color-Widgets/include/bound_color_selector.hpp
    third_party/Qt-Color-Widgets/include/color_2d_slider.hpp
    third_party/Qt-Color-Widgets/include/color_delegate.hpp
    third_party/Qt-Color-Widgets/include/color_dialog.hpp
    third_party/Qt-Color-Widgets/include/color_line_edit.hpp
    third_party/Qt-Color-Widgets/include/color_list_widget.hpp
    third_party/Qt-Color-Widgets/include/color_names.hpp
    third_party/Qt-Color-Widgets/include/color_palette.hpp
    third_party/Qt-Color-Widgets/include/color_palette_model.hpp
    third_party/Qt-Color-Widgets/include/color_palette_widget.hpp
    third_party/Qt-Color-Widgets/include/color_preview.hpp
    third_party/Qt-Color-Widgets/include/color_selector.hpp
    third_party/Qt-Color-Widgets/include/color_wheel.hpp
    third_party/Qt-Color-Widgets/include/colorwidgets_global.hpp
    third_party/Qt-Color-Widgets/include/gradient_slider.hpp
    third_party/Qt-Color-Widgets/include/hue_slider.hpp
    third_party/Qt-Color-Widgets/include/swatch.hpp
)
target_compile_definitions(Editor PRIVATE "QTCOLORWIDGETS_LIBRARY")
target_include_directories(Editor PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor")
# add this include for the promoted widgets
target_include_directories(Editor PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/gui")
target_include_directories(Editor PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/Qt-Color-Widgets/include")
# support Clion finding the generated headers (see AUTOGEN_BUILD_DIR)
target_include_directories(Editor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Editor_autogen/include")
target_link_libraries(Editor Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(Editor freetype_custom_build)
target_link_libraries(Editor harfbuzz_custom_build)
if (UNIX)
    target_link_libraries(Editor ncurses)
endif()
install(TARGETS Editor DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/editor/dist")
install(DIRECTORY    "${CMAKE_CURRENT_LIST_DIR}/graphics/shaders"
        DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/editor/dist")

# game host application which provides a host process to load the game
# libraries for playback.
add_executable(EditorGameHost
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    graphics/drawable.cpp
    graphics/material.cpp
    graphics/image.cpp
    graphics/text.cpp
    graphics/resource.cpp
    gamelib/animation.cpp
    editor/app/format.cpp
    editor/app/utility.cpp
    editor/app/eventlog.h
    editor/app/eventlog.cpp
    editor/app/packing.cpp
    editor/app/workspace.h
    editor/app/workspace.cpp
    editor/app/ipc.h
    editor/app/ipc.cpp
    editor/gui/host.cpp
    editor/gui/utility.h
    editor/gui/playwindow.h
    editor/gui/playwindow.ui
    editor/gui/playwindow.cpp
    editor/resource/resource.qrc
    editor/resource/qdarkstyle/style.qrc
    third_party/base64/base64.cpp
    third_party/stb/stb_image_write.c
    third_party/stb/stb_image.c
)
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor")
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/gui")
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/Qt-Color-Widgets/include")
# support Clion finding the generated headers (see AUTOGEN_BUILD_DIR)
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/EditorGameHost_autogen/include")
if (UNIX)
    target_link_libraries(EditorGameHost ncurses)
endif()
target_link_libraries(EditorGameHost Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(EditorGameHost freetype_custom_build)
target_link_libraries(EditorGameHost harfbuzz_custom_build)
install(TARGETS EditorGameHost DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/editor/dist")

# audio test application. Simple console application that plays
# OGG and WAV files in several different formats.
add_executable(audio_test
    audio/pulseaudio.cpp
    audio/waveout.cpp
    audio/sample.cpp
    audio/player.cpp
    audio/test/main.cpp
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
)
target_include_directories(audio_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio/test")
target_link_libraries(audio_test Qt5::Widgets)
if (WIN32)
    target_link_libraries(audio_test "${CMAKE_CURRENT_LIST_DIR}/third_party/libsndfile/libsndfile-1.lib")
elseif (UNIX)
    target_link_libraries(audio_test ncurses)
    target_link_libraries(audio_test pulse)
    target_link_libraries(audio_test sndfile)
    target_link_libraries(audio_test pthread)
endif()
install(TARGETS audio_test DESTINATION "${CMAKE_CURRENT_LIST_DIR}/audio/test/")

# graphics test application. Uses the graphics APIs to draw stuff.
add_executable(graphics_test
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    graphics/material.cpp
    graphics/drawable.cpp
    graphics/drawing.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/resource.cpp
    graphics/image.cpp
    graphics/test/main.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp
)
target_include_directories(graphics_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/test")
target_link_libraries(graphics_test freetype_custom_build)
target_link_libraries(graphics_test harfbuzz_custom_build)
target_link_libraries(graphics_test wdk_system wdk_desktop_gl)
install(TARGETS graphics_test DESTINATION "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")
install(DIRECTORY    "${CMAKE_CURRENT_LIST_DIR}/graphics/shaders"
        DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")
if (UNIX)
    target_link_libraries(graphics_test ncurses)
endif()


# main game runner application. The executable will read a
# config.json and create the window/context for the application as
# per the configuration. The game logic will be loaded from a .so or .dll
add_executable(main
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    gamelib/main/main.cpp)
target_include_directories(main PRIVATE "${CMAKE_CURRENT_LIST_DIR}/gamelib/main")
target_link_libraries(main wdk_system wdk_desktop_gl)
if (UNIX)
    target_link_libraries(main ncurses dl pthread)
endif()

# gamelib test application.
add_library(gamelib_test SHARED
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    graphics/material.cpp
    graphics/drawable.cpp
    graphics/drawing.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/resource.cpp
    graphics/image.cpp
    graphics/opengl_es2_device.cpp
    gamelib/animation.cpp
    gamelib/loader.cpp
    gamelib/test/main.cpp
    gamelib/main/gamelib.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp
)
target_include_directories(gamelib_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/gamelib/test")

target_link_libraries(gamelib_test PRIVATE freetype_custom_build harfbuzz_custom_build)
install(TARGETS gamelib_test main DESTINATION "${CMAKE_CURRENT_LIST_DIR}/gamelib/test/dist")
install(DIRECTORY    "${CMAKE_CURRENT_LIST_DIR}/graphics/shaders"
        DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/gamelib/test/dist")
# hide symbols on linux
if (CMAKE_COMPILER_IS_GNUCC)
    target_link_options(gamelib_test PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/linux_linker_gamelib.txt")
endif()


# game project
add_library(invaders SHARED
    games/invaders/level.cpp
    games/invaders/gamewidget.cpp
    games/invaders/game.cpp
    games/invaders/main.cpp
    audio/pulseaudio.cpp
    audio/waveout.cpp
    audio/sample.cpp
    audio/player.cpp
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    graphics/material.cpp
    graphics/image.cpp
    graphics/drawable.cpp
    graphics/drawing.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/resource.cpp
    gamelib/homedir.cpp
    gamelib/animation.cpp
    gamelib/loader.cpp
    gamelib/main/gamelib.cpp
    wdk/wdk/keys.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp
)
target_include_directories(invaders PRIVATE "${CMAKE_CURRENT_LIST_DIR}/games/invaders")
target_link_libraries(invaders PRIVATE freetype_custom_build harfbuzz_custom_build)
if (WIN32)
  include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/libsndfile")
  target_link_libraries(invaders PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/libsndfile/libsndfile-1.lib")
elseif (UNIX)
  target_link_libraries(invaders PRIVATE pulse)
  target_link_libraries(invaders PRIVATE sndfile)
  target_link_libraries(invaders PRIVATE pthread)
endif()
install(TARGETS invaders main DESTINATION "${CMAKE_CURRENT_LIST_DIR}/games/invaders/dist")
# hide symbols on linux
if (CMAKE_COMPILER_IS_GNUCC)
    target_link_options(invaders PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/linux_linker_gamelib.txt")
endif()

# unit tests
enable_testing()

#editor unit tests
add_executable(unit_test_imgpack  editor/app/unit_test/unit_test_image_packing.cpp
                                  editor/app/packing.cpp
                                  base/assert.cpp
                                  third_party/stb/stb_image.c
                                  third_party/stb/stb_image_write.c)
add_executable(unit_test_ipc editor/app/unit_test/unit_test_ipc.cpp
        base/assert.cpp
        base/logging.cpp
        graphics/material.cpp
        graphics/resource.cpp
        graphics/text.cpp
        graphics/drawable.cpp
        graphics/image.cpp
        gamelib/animation.cpp
        editor/app/eventlog.cpp
        editor/app/format.cpp
        editor/app/utility.cpp
        editor/app/workspace.cpp
        editor/app/ipc.cpp
        editor/app/packing.cpp
        third_party/base64/base64.cpp
        third_party/stb/stb_image.c)
add_test(NAME unit_test_imgpack COMMAND unit_test_imgpack)
add_test(NAME unit_test_ipc     COMMAND unit_test_icp)
target_include_directories(unit_test_imgpack  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_include_directories(unit_test_ipc      PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_link_libraries(unit_test_ipc Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(unit_test_ipc freetype_custom_build)
target_link_libraries(unit_test_ipc harfbuzz_custom_build)

# gamelib tests
add_executable(unit_test_settings gamelib/unit_test/unit_test_settings.cpp)
add_executable(unit_test_tree     gamelib/unit_test/unit_test_tree.cpp  base/assert.cpp)
add_executable(unit_test_anim     gamelib/unit_test/unit_test_animation.cpp
    base/assert.cpp
    base/logging.cpp
    graphics/drawable.cpp
    graphics/material.cpp
    graphics/image.cpp
    graphics/text.cpp
    graphics/resource.cpp
    gamelib/animation.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp)
target_link_libraries(unit_test_anim
        freetype_custom_build
        harfbuzz_custom_build)

target_include_directories(unit_test_tree     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/gamelib/unit_test")
target_include_directories(unit_test_anim     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/gamelib/unit_test")
target_include_directories(unit_test_settings PRIVATE "${CMAKE_CURRENT_LIST_DIR}/gamelib/unit_test")
add_test(NAME unit_test_tree COMMAND unit_test_tree)
add_test(NAME unit_test_anim COMMAND unit_test_anim)
add_test(NAME unit_test_settings COMMAND unit_test_settings)

# base tests
add_executable(unit_test_math base/unit_test/unit_test_math.cpp)
add_executable(unit_test_cmdline base/unit_test/unit_test_cmdline.cpp)
add_executable(unit_test_logging base/unit_test/unit_test_log.cpp base/logging.cpp)
target_include_directories(unit_test_logging PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test/")
if (UNIX)
   target_link_libraries(unit_test_logging PRIVATE pthread)
endif()
target_include_directories(unit_test_math     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test")
add_test(NAME unit_test_math COMMAND unit_test_math)
add_test(NAME unit_test_cmdline COMMAND unit_test_cmdline)
add_test(NAME unit_test_logging COMMAND unit_test_logging)

# gfx tests
add_executable(unit_test_bitmap   graphics/unit_test/unit_test_bitmap.cpp base/assert.cpp third_party/stb/stb_image_write.c)
add_executable(unit_test_image    graphics/unit_test/unit_test_image.cpp  base/assert.cpp third_party/stb/stb_image.c
        graphics/image.cpp
        graphics/resource.cpp)
add_executable(unit_test          graphics/unit_test/unit_test.cpp graphics/drawable.cpp base/assert.cpp)
add_executable(unit_test_device   graphics/unit_test/unit_test_device.cpp base/assert.cpp
        graphics/opengl_es2_device.cpp
        graphics/resource.cpp
        third_party/stb/stb_image_write.c)
target_link_libraries(unit_test_device
    wdk_system
    wdk_desktop_gl
    freetype_custom_build
    harfbuzz_custom_build)

add_test(NAME unit_test_bitmap COMMAND unit_test_bitmap)
add_test(NAME unit_test_image COMMAND unit_test_image)
add_test(NAME unit_test COMMAND unit_test)
add_test(NAME unit_test_device COMMAND unit_test_device)
target_include_directories(unit_test_bitmap   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_image    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test          PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_device   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")









