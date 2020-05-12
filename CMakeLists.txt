cmake_minimum_required(VERSION 3.0)

project(PinyinInvaders)

message(STATUS "
          ~~ PinyinInvaders ~~

    \\\\o Brought to you by Ensisoft o//
        http://www.ensisoft.com
    Copyright (c) 2019 Sami Väisänen
                Ensisoft

https://github.com/ensisoft/pinyin-invaders

")

# begin with the turds in 3rd party libs

# Freetype2
option(WITH_ZLIB "Freetype: Enable zlib " OFF)
option(WITH_BZip2 "Freetype: Enable bzip2 " OFF)
option(WITH_HarfBuzz "Freetype: Enable harfbuzz" OFF)
option(WITH_PNG "Freetype: Build with PNG" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype)

# harfbuzz (depends on freetype2)
set(HARFBUZZ_FREETYPE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype/include" CACHE STRING "Freetype2 include folder")
set(HARFBUZZ_FREETYPE_LIBRARY "freetype_custom_build" CACHE STRING "Name of the Freetype2 library build.")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz)

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
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    scene/animation.cpp
    editor/app/event.h
    editor/app/eventlog.h
    editor/app/eventlog.cpp
    editor/app/format.h
    editor/app/utility.h
    editor/app/utility.cpp
    editor/app/workspace.h
    editor/app/workspace.cpp
    editor/gui/animationwidget.h
    editor/gui/animationwidget.ui
    editor/gui/animationwidget.cpp
    editor/gui/childwindow.h
    editor/gui/childwindow.cpp
    editor/gui/childwindow.ui
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
    editor/gui/materialwidget.h
    editor/gui/materialwidget.cpp
    editor/gui/materialwidget.ui
    editor/gui/settings.h
    editor/gui/settings.cpp
    editor/gui/treewidget.h
    editor/gui/treewidget.cpp
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
target_link_libraries(Editor Qt5::Widgets Qt5::OpenGL Qt5::Core)
target_link_libraries(Editor freetype_custom_build)
target_link_libraries(Editor harfbuzz_custom_build)
if (UNIX)
    target_link_libraries(Editor ncurses)
endif()
install(TARGETS Editor DESTINATION "${CMAKE_CURRENT_LIST_DIR}/editor/dist")
install(DIRECTORY    "${CMAKE_CURRENT_LIST_DIR}/graphics/shaders"
        DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/editor/dist")


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
    graphics/drawable.cpp
    graphics/drawing.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/test/main.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
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

# game project
add_executable(invaders
    game/level.cpp
    game/gamewidget.cpp
    game/game.cpp
    game/main.cpp
    audio/pulseaudio.cpp
    audio/waveout.cpp
    audio/sample.cpp
    audio/player.cpp
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    graphics/drawable.cpp
    graphics/drawing.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
)
target_include_directories(invaders PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game")
target_link_libraries(invaders Qt5::Widgets)
target_link_libraries(invaders freetype_custom_build)
target_link_libraries(invaders harfbuzz_custom_build)
target_link_libraries(invaders wdk_system wdk_desktop_gl)
if (WIN32)
  include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/libsndfile")
  target_link_libraries(invaders "${CMAKE_CURRENT_LIST_DIR}/third_party/libsndfile/libsndfile-1.lib")
elseif (UNIX)
  target_link_libraries(invaders ncurses)
  target_link_libraries(invaders pulse)
  target_link_libraries(invaders sndfile)
  target_link_libraries(invaders pthread)
endif()
install(TARGETS invaders DESTINATION "${CMAKE_CURRENT_LIST_DIR}/game/dist")
install(DIRECTORY    "${CMAKE_CURRENT_LIST_DIR}/graphics/shaders"
        DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/game/dist")


# unit tests
enable_testing()
add_executable(unit_test_bitmap graphics/unit_test/unit_test_bitmap.cpp base/assert.cpp)
add_executable(unit_test_image  graphics/unit_test/unit_test_image.cpp base/assert.cpp third_party/stb/stb_image.c)
add_executable(unit_test        graphics/unit_test/unit_test.cpp)
add_executable(unit_test_device graphics/unit_test/unit_test_device.cpp
    base/assert.cpp
    graphics/opengl_es2_device.cpp
    third_party/stb/stb_image_write.c)
add_executable(unit_test_tree
    base/assert.cpp
    scene/unit_test/unit_test_tree.cpp )

add_executable(unit_test_math
    base/unit_test/unit_test_math.cpp)

target_link_libraries(unit_test_device
    wdk_system
    wdk_desktop_gl
    freetype_custom_build
    harfbuzz_custom_build)
target_include_directories(unit_test_bitmap PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_image  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_device PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")

target_include_directories(unit_test_tree PRIVATE "${CMAKE_CURRENT_LIST_DIR}/scene/unit_test")

target_include_directories(unit_test_math PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test")
