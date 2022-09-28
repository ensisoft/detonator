cmake_minimum_required(VERSION 3.0)

project(Gamestudio)

option(USE_MOLD_LINKER "Use mold linker. https://github.com/rui314/mold" OFF)

message(STATUS "
          ~~ Ensisoft GameStudio ~~

    \\\\o Brought to you by Ensisoft o//
        http://www.ensisoft.com
    Copyright (c) 2019 Sami Väisänen
                Ensisoft

https://github.com/ensisoft/gamestudio

")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# wdk
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/wdk)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party)
# current version is 0.2.1
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsamplerate)
# current version is 1.0.31
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsndfile)
# current version 2.4.1
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/box2d)

include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/wdk")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/glm")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/Khronos")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/libsamplerate/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/libsndfile/include")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/lua/src")
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/third_party/box2d/include")

# this is only for CLion right now
if (UNIX)
    include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/emsdk/upstream/emscripten/system/include")
endif()

# see this bug report about C++14 and Qt
# https://bugreports.qt.io/browse/QTBUG-53002
set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Building with Clang")
    # in case of using something like Ninja we need this explicitly.
    # https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
    add_compile_options (-fcolor-diagnostics)

    # neargye magic-enum exceeds maximum expression nesting limit
    add_compile_options(-fbracket-depth=1024)

    if (USE_MOLD_LINKER)
        message(STATUS "Using mold linker https://github.com/rui314/mold")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "-fuse-ld=/usr/bin/mold")
        add_link_options(-fuse-ld=mold)
    endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Building with GCC")
    # in case of using something like Ninja we need this explicitly.
    # https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
    add_compile_options(-fdiagnostics-color=always)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "Building with MSVC")
    # /wd4251 Qt dll interface crap
    # /wd4275 Qt dll interface crap again
    # /wd4018 signed/unsigned mismatch
    # /wd4244 conversion from double to float
    # /wd4305 truncation from double to float
    # /wd4267 conversion from size_t to int (Qt uses ints extensively)
    add_definitions("/wd4251 /wd4275 /wd4018 /wd4244 /wd4305 /wd4267")

    # generate debugging .pdb files for Release builds on MSVS
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        #  /Zi turns on the .pdb generation. We want this for the release build
        # so that we can do post-mortem debugging with the minidump (from the crash)
        # and the pdb file.
        # /FS is needed for parallel build. It forces data to be written
        # through a serialized surrogate process.
        # NOTE that the /Zi names the .pdb file as vcx0.pdb where the x is
        # the version number. So for example vc120.pdb
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS")
        # http://www.wintellect.com/devcenter/jrobbins/correctly-creating-native-c-release-build-pdbs
        # need to tell the linker also about the debug data.
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:ICF")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:ICF")
    endif()
endif()

# we also need Qt for the time being
if (WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/msvc2019_64")
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
# Disable any prefix on debug libraries. This should make it easier to
# switch between debug/release versions when loading the game engine library.
# I.e the way the library is loaded based on its configured name doesn't
# need to change from release to debug.
set(CMAKE_DEBUG_POSTFIX "")

# combine as many translation units as possible into static libraries
# in order to reduce the build times by not repeatedly building a translation
# unit when it's possible to use a library
add_library(BaseLib
    base/assert.cpp
    base/format.cpp
    base/logging.cpp
    base/json.cpp
    base/utility.cpp
    base/trace.cpp)
add_library(AudioLib
    audio/format.cpp
    audio/loader.cpp
    audio/sndfile.cpp
    audio/mpg123.cpp
    audio/pulseaudio.cpp
    audio/waveout.cpp
    audio/openal.cpp
    audio/source.cpp
    audio/player.cpp
    audio/element.cpp
    audio/graph.cpp)
add_library(DataLib
    data/json.cpp)
add_library(GfxLib
    graphics/bitmap.cpp
    graphics/drawing.cpp
    graphics/drawable.cpp
    graphics/material.cpp
    graphics/image.cpp
    graphics/device.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/loader.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp)
add_library(EngineLib
    game/animation.cpp
    game/entity.cpp
    game/scriptvar.cpp
    game/scene.cpp
    game/tilemap.cpp
    engine/audio.cpp
    engine/renderer.cpp
    engine/physics.cpp
    engine/ui.cpp
    engine/state.cpp)
add_library(UiLib
    uikit/layout.cpp
    uikit/widget.cpp
    uikit/window.cpp)

add_library(GfxLibTesting
    graphics/bitmap.cpp
    graphics/drawing.cpp
    graphics/drawable.cpp
    graphics/material.cpp
    graphics/image.cpp
    graphics/device.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/loader.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp)
target_include_directories(GfxLibTesting PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
if (UNIX)
    target_compile_options(GfxLibTesting PRIVATE -fPIC)
endif()

target_include_directories(AudioLib  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(BaseLib   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(DataLib   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(GfxLib    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(EngineLib PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(UiLib     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
if (UNIX)
    target_compile_options(AudioLib  PRIVATE -fPIC)
    target_compile_options(DataLib   PRIVATE -fPIC)
    target_compile_options(BaseLib   PRIVATE -fPIC)
    target_compile_options(GfxLib    PRIVATE -fPIC)
    target_compile_options(EngineLib PRIVATE -fPIC)
    target_compile_options(UiLib     PRIVATE -fPIC)

    target_link_libraries(AudioLib INTERFACE pulse)
    target_link_libraries(AudioLib INTERFACE pthread)
    target_link_libraries(AudioLib INTERFACE samplerate)
    target_link_libraries(AudioLib INTERFACE sndfile)
endif()
if (MSVC)
   target_compile_options(EngineLib  INTERFACE /bigobj)

   target_link_libraries(AudioLib INTERFACE samplerate)
   target_link_libraries(AudioLib INTERFACE sndfile)
endif()

target_link_libraries(EngineLib INTERFACE box2d)

add_library(QtColorWidgets
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
        third_party/Qt-Color-Widgets/include/swatch.hpp)
target_include_directories(QtColorWidgets PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/Qt-Color-Widgets/include")
target_compile_definitions(QtColorWidgets PRIVATE "QTCOLORWIDGETS_LIBRARY")
target_link_libraries(QtColorWidgets Qt5::Widgets Qt5::Core Qt5::CorePrivate)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # suppress some noise coming from Qt (Qt-Color-Widgets)
    target_compile_options(QtColorWidgets PRIVATE -Wno-deprecated-declarations)
endif()

# Editor application.
add_executable(GSEditor
    editor/app/event.h
    editor/app/eventlog.h
    editor/app/eventlog.cpp
    editor/app/window-eventlog.h
    editor/app/window-eventlog.cpp
    editor/app/format.h
    editor/app/format.cpp
    editor/app/packing.h
    editor/app/packing.cpp
    editor/app/utility.h
    editor/app/utility.cpp
    editor/app/workspace.h
    editor/app/workspace.cpp
    editor/app/buffer.h
    editor/app/buffer.cpp
    editor/app/packing.cpp
    editor/app/process.h
    editor/app/process.cpp
    editor/app/ipc.cpp
    editor/gui/audiowidget.h
    editor/gui/audiowidget.ui
    editor/gui/audiowidget.cpp
    editor/gui/animationtrackwidget.h
    editor/gui/animationtrackwidget.ui
    editor/gui/animationtrackwidget.cpp
    editor/gui/childwindow.h
    editor/gui/childwindow.cpp
    editor/gui/childwindow.ui
    editor/gui/codewidget.h
    editor/gui/codewidget.cpp
    editor/gui/drawing.cpp
    editor/gui/playwindow.h
    editor/gui/playwindow.cpp
    editor/gui/playwindow.ui
    editor/gui/dlgeventlog.h
    editor/gui/dlgeventlog.ui
    editor/gui/dlgeventlog.cpp
    editor/gui/dlgabout.h
    editor/gui/dlgabout.ui
    editor/gui/dlgabout.cpp
    editor/gui/dlgjoint.h
    editor/gui/dlgjoint.ui
    editor/gui/dlgjoint.cpp
    editor/gui/dlgsettings.h
    editor/gui/dlgsettings.ui
    editor/gui/dlgsettings.cpp
    editor/gui/dlgentity.h
    editor/gui/dlgentity.ui
    editor/gui/dlgentity.cpp
    editor/gui/dlgbitmap.h
    editor/gui/dlgbitmap.cpp
    editor/gui/dlgtext.h
    editor/gui/dlgtext.cpp
    editor/gui/dlgtext.ui
    editor/gui/dlgfont.h
    editor/gui/dlgfont.ui
    editor/gui/dlgfont.cpp
    editor/gui/dlgtexturerect.h
    editor/gui/dlgtexturerect.ui
    editor/gui/dlgtexturerect.cpp
    editor/gui/dlgimgpack.ui
    editor/gui/dlgimgpack.h
    editor/gui/dlgimgpack.cpp
    editor/gui/dlgmaterial.h
    editor/gui/dlgmaterial.ui
    editor/gui/dlgmaterial.cpp
    editor/gui/dlgmaterialparams.h
    editor/gui/dlgmaterialparams.ui
    editor/gui/dlgmaterialparams.cpp
    editor/gui/dlgpackage.h
    editor/gui/dlgpackage.ui
    editor/gui/dlgpackage.cpp
    editor/gui/dlgproject.h
    editor/gui/dlgproject.ui
    editor/gui/dlgproject.cpp
    editor/gui/dlgsave.h
    editor/gui/dlgsave.ui
    editor/gui/dlgsave.cpp
    editor/gui/dlgopen.h
    editor/gui/dlgopen.ui
    editor/gui/dlgopen.cpp
    editor/gui/dlgscriptvar.h
    editor/gui/dlgscriptvar.ui
    editor/gui/dlgscriptvar.cpp
    editor/gui/dlgstyleproperties.h
    editor/gui/dlgstyleproperties.ui
    editor/gui/dlgstyleproperties.cpp
    editor/gui/dlgstylestring.h
    editor/gui/dlgstylestring.ui
    editor/gui/dlgstylestring.cpp
    editor/gui/dlggradient.h
    editor/gui/dlggradient.ui
    editor/gui/dlgnew.h
    editor/gui/dlgnew.ui
    editor/gui/dlgtilelayer.h
    editor/gui/dlgtilelayer.ui
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
    editor/gui/entitywidget.h
    editor/gui/entitywidget.ui
    editor/gui/entitywidget.cpp
    editor/gui/scenewidget.h
    editor/gui/scenewidget.ui
    editor/gui/scenewidget.cpp
    editor/gui/scriptwidget.h
    editor/gui/scriptwidget.ui
    editor/gui/scriptwidget.cpp
    editor/gui/settings.h
    editor/gui/settings.cpp
    editor/gui/treewidget.h
    editor/gui/treewidget.cpp
    editor/gui/timelinewidget.h
    editor/gui/timelinewidget.cpp
    editor/gui/rangewidget.h
    editor/gui/rangewidget.cpp
    editor/gui/uiwidget.h
    editor/gui/uiwidget.ui
    editor/gui/uiwidget.cpp
    editor/gui/tilemapwidget.h
    editor/gui/tilemapwidget.ui
    editor/gui/tilemapwidget.cpp
    editor/gui/uniform.h
    editor/gui/uniform.ui
    editor/gui/uniform.cpp
    editor/gui/palettematerial.h
    editor/gui/palettematerial.ui
    editor/gui/palettematerial.cpp
    editor/gui/sampler.h
    editor/gui/sampler.ui
    editor/gui/widgetstylewidget.h
    editor/gui/widgetstylewidget.ui
    editor/gui/widgetstylewidget.cpp
    editor/gui/timewidget.h
    editor/gui/timewidget.ui
    editor/gui/timewidget.cpp
    editor/gui/utility.cpp
    editor/resource/resource.qrc
    editor/welcome.qrc
    third_party/QSourceHighlight/languagedata.cpp
    third_party/QSourceHighlight/qsourcehighliter.cpp
    third_party/QSourceHighlight/qsourcehighliterthemes.cpp
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # suppress some noise coming from Qt (Qt-Color-Widgets)
    target_compile_options(GSEditor PRIVATE -Wno-deprecated-declarations)
endif()

target_compile_definitions(GSEditor PRIVATE "QTCOLORWIDGETS_LIBRARY")
target_include_directories(GSEditor PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor")
# add this include for the promoted widgets
target_include_directories(GSEditor PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/gui")
target_include_directories(GSEditor PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/Qt-Color-Widgets/include")
# support Clion finding the generated headers (see AUTOGEN_BUILD_DIR)
target_include_directories(GSEditor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Editor_autogen/include")
target_link_libraries(GSEditor Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(GSEditor EngineLib UiLib GfxLib AudioLib DataLib BaseLib)
target_link_libraries(GSEditor Lua)
target_link_libraries(GSEditor QtColorWidgets)
target_link_libraries(GSEditor ${CONAN_LIBS})
install(TARGETS GSEditor DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/editor/dist")

# game host application which provides a host process to load the game
# libraries for playback.
add_executable(EditorGameHost
    editor/app/format.cpp
    editor/app/utility.cpp
    editor/app/eventlog.h
    editor/app/eventlog.cpp
    editor/app/packing.cpp
    editor/app/workspace.h
    editor/app/workspace.cpp
    editor/app/buffer.cpp
    editor/app/window-eventlog.cpp
    editor/app/ipc.h
    editor/app/ipc.cpp
    editor/app/process.cpp
    editor/gui/host.cpp
    editor/gui/utility.h
    editor/gui/playwindow.h
    editor/gui/playwindow.ui
    editor/gui/playwindow.cpp
    editor/gui/dlgeventlog.h
    editor/gui/dlgeventlog.ui
    editor/gui/dlgeventlog.cpp
    editor/resource/resource.qrc
)
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor")
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/gui")
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/Qt-Color-Widgets/include")
# support Clion finding the generated headers (see AUTOGEN_BUILD_DIR)
target_include_directories(EditorGameHost PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/EditorGameHost_autogen/include")
target_link_libraries(EditorGameHost Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(EditorGameHost EngineLib AudioLib UiLib GfxLib DataLib BaseLib)
target_link_libraries(EditorGameHost ${CONAN_LIBS})
install(TARGETS EditorGameHost DESTINATION  "${CMAKE_CURRENT_LIST_DIR}/editor/dist")

# audio test application. Simple console application that plays
# OGG and WAV files in several different formats.
add_executable(audio_test
    audio/format.cpp
    audio/loader.cpp
    audio/sndfile.cpp
    audio/mpg123.cpp
    audio/pulseaudio.cpp
    audio/waveout.cpp
    audio/openal.cpp
    audio/source.cpp
    audio/player.cpp
    audio/element.cpp
    audio/graph.cpp
    audio/test/main.cpp
)
target_include_directories(audio_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio/test")
target_link_libraries(audio_test BaseLib)
target_link_libraries(audio_test Qt5::Widgets)
target_link_libraries(audio_test ${CONAN_LIBS})
target_link_libraries(audio_test samplerate)
target_link_libraries(audio_test sndfile)
if (UNIX)
    target_link_libraries(audio_test pulse)
    target_link_libraries(audio_test pthread)
#    target_link_libraries(audio_test openal)
endif()
install(TARGETS audio_test DESTINATION "${CMAKE_CURRENT_LIST_DIR}/audio/test/")

# graphics test application. Uses the graphics APIs to draw stuff.
# needs DETERMINISTIC_RANDOM flag
add_executable(graphics_test
    graphics/bitmap.cpp
    graphics/material.cpp
    graphics/drawable.cpp
    graphics/drawing.cpp
    graphics/device.cpp
    graphics/opengl_es2_device.cpp
    graphics/painter.cpp
    graphics/text.cpp
    graphics/loader.cpp
    graphics/image.cpp
    graphics/test/main.cpp
    third_party/stb/stb_image.c
    third_party/stb/stb_image_write.c
    third_party/base64/base64.cpp
)
target_include_directories(graphics_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/test")
target_link_libraries(graphics_test BaseLib)
target_link_libraries(graphics_test ${CONAN_LIBS})
target_link_libraries(graphics_test wdk_system wdk_desktop_gl)
install(TARGETS graphics_test DESTINATION "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")

# main game runner application. The executable will read a
# config.json and create the window/context for the application as
# per the configuration. The game logic will be loaded from a .so or .dll
add_executable(GameMain engine/main/main.cpp)
target_include_directories(GameMain PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/main")
target_link_libraries(GameMain DataLib BaseLib wdk_system wdk_desktop_gl)
if (UNIX)
    target_link_libraries(GameMain dl pthread)
endif()
install(TARGETS GameMain DESTINATION "${CMAKE_CURRENT_LIST_DIR}/editor/dist")

# generic game engine library.
add_library(GameEngine SHARED
    engine/loader.cpp
    engine/engine.cpp
    engine/lua.cpp
    engine/main/gamelib.cpp
)
target_include_directories(GameEngine PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/main")
target_link_libraries(GameEngine PRIVATE GfxLib EngineLib AudioLib UiLib DataLib BaseLib)
target_link_libraries(GameEngine PRIVATE ${CONAN_LIBS})
target_link_libraries(GameEngine PRIVATE wdk_system)
target_link_libraries(GameEngine PRIVATE Lua)
install(TARGETS GameEngine DESTINATION "${CMAKE_CURRENT_LIST_DIR}/editor/dist")
# hide symbols on linux
if (UNIX)
    target_link_options(GameEngine PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/linux_linker_gamelib.txt")
endif()

# test engine
add_library(TestEngine SHARED
    engine/loader.cpp
    engine/test/main.cpp
    engine/main/gamelib.cpp
)
target_include_directories(TestEngine PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/test")
target_link_libraries(TestEngine PRIVATE GfxLib EngineLib AudioLib UiLib DataLib BaseLib)
target_link_libraries(TestEngine PRIVATE ${CONAN_LIBS})
install(TARGETS TestEngine GameMain DESTINATION "${CMAKE_CURRENT_LIST_DIR}/engine/test/dist")

# hide symbols on linux
if (UNIX)
    target_link_options(TestEngine PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/linux_linker_gamelib.txt")
endif()

# engine perf test app
add_executable(perf-test engine/test/perftest.cpp)
target_include_directories(perf-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/test")
target_link_libraries(perf-test PRIVATE GfxLibTesting EngineLib GfxLibTesting AudioLib UiLib DataLib BaseLib)
target_link_libraries(perf-test PRIVATE ${CONAN_LIBS})
target_link_libraries(perf-test PRIVATE wdk_system wdk_desktop_gl)
target_link_libraries(perf-test PRIVATE Lua)
install(TARGETS perf-test DESTINATION "${CMAKE_CURRENT_LIST_DIR}/engine/test/dist")

# simple standalone executable to play with lua and sol
# for testing c++/lua integration strategies.
add_executable(lua-test engine/test/luatest.cpp)
target_include_directories(lua-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/test")
target_link_libraries(lua-test PRIVATE Lua ${CONAN_LIBS})

# unit tests
enable_testing()

#data unit tests
add_executable(unit_test_data data/unit_test/unit_test.cpp
        data/json.cpp)
add_test(NAME unit_test_data COMMAND unit_test_data)
target_include_directories(unit_test_data PRIVATE "${CMAKE_CURRENT_LIST_DIR}/data/unit_test")
target_link_libraries(unit_test_data PRIVATE BaseLib)

#editor unit tests
add_executable(unit_test_imgpack
        editor/app/unit_test/unit_test_image_packing.cpp
        editor/app/packing.cpp
        base/assert.cpp
        graphics/bitmap.cpp
        third_party/stb/stb_image.c
        third_party/stb/stb_image_write.c)
add_executable(unit_test_ipc
        editor/app/unit_test/unit_test_ipc.cpp
        editor/app/eventlog.cpp
        editor/app/format.cpp
        editor/app/utility.cpp
        editor/app/workspace.cpp
        editor/app/buffer.cpp
        editor/app/ipc.cpp
        editor/app/packing.cpp
        editor/app/process.cpp)
add_executable(unit_test_resource
        editor/app/unit_test/unit_test_resource.cpp
        editor/app/utility.cpp
        base/assert.cpp)
add_executable(unit_test_workspace
        editor/app/unit_test/unit_test_workspace.cpp
        engine/loader.cpp
        editor/app/utility.cpp
        editor/app/format.cpp
        editor/app/eventlog.cpp
        editor/app/workspace.cpp
        editor/app/buffer.cpp
        editor/app/packing.cpp
        editor/app/process.cpp)
add_executable(unit_test_event_log
        editor/app/unit_test/unit_test_window-eventlog.cpp
        editor/app/window-eventlog.cpp)

add_executable(unit_test_gui
        base/assert.cpp
        base/json.cpp
        base/utility.cpp
        data/json.cpp
        editor/app/utility.cpp
        editor/app/eventlog.cpp
        editor/gui/settings.cpp
        editor/gui/unit_test/unit_test.cpp
        third_party/base64/base64.cpp)
target_compile_definitions(unit_test_gui PRIVATE "QTCOLORWIDGETS_LIBRARY")
target_include_directories(unit_test_gui PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor")
# add this include for the promoted widgets
target_include_directories(unit_test_gui PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/gui")
target_include_directories(unit_test_gui PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/Qt-Color-Widgets/include")

add_test(NAME unit_test_gui       COMMAND unit_test_gui)
add_test(NAME unit_test_imgpack   COMMAND unit_test_imgpack)
add_test(NAME unit_test_ipc       COMMAND unit_test_ipc)
add_test(NAME unit_test_resource  COMMAND unit_test_resource)
add_test(NAME unit_test_workspace COMMAND unit_test_workspace)
add_test(NAME unit_test_event_log COMMAND unit_test_event_log)
target_include_directories(unit_test_gui       PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_include_directories(unit_test_imgpack   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_include_directories(unit_test_ipc       PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_include_directories(unit_test_resource  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_include_directories(unit_test_workspace PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_include_directories(unit_test_event_log PRIVATE "${CMAKE_CURRENT_LIST_DIR}/editor/app/unit_test")
target_link_libraries(unit_test_gui QtColorWidgets Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(unit_test_ipc Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::CorePrivate Qt5::Network)
target_link_libraries(unit_test_ipc EngineLib AudioLib UiLib GfxLib DataLib BaseLib ${CONAN_LIBS})
target_link_libraries(unit_test_resource Qt5::Core Qt5::Widgets UiLib DataLib BaseLib)
target_link_libraries(unit_test_workspace EngineLib AudioLib UiLib GfxLib DataLib BaseLib)
target_link_libraries(unit_test_workspace Qt5::Core Qt5::Widgets Qt5::CorePrivate Qt5::Network ${CONAN_LIBS})
target_link_libraries(unit_test_event_log Qt5::Core DataLib BaseLib)

add_library(GameLibTesting
    game/entity.cpp
    game/scriptvar.cpp
    game/animation.cpp
    game/scene.cpp
    game/tilemap.cpp)
add_library(EngineLibTesting
    engine/audio.cpp
    engine/physics.cpp
    engine/renderer.cpp
    engine/lua.cpp
    engine/state.cpp)
target_include_directories(GameLibTesting   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(EngineLibTesting PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/unit_test")
if (MSVC)
    target_compile_options(EngineLibTesting PRIVATE /bigobj)
endif()

target_link_libraries(EngineLibTesting INTERFACE box2d)

add_executable(unit_test_tree      game/unit_test/unit_test_tree.cpp)
add_executable(unit_test_anim      game/unit_test/unit_test_animation.cpp)
add_executable(unit_test_entity    game/unit_test/unit_test_entity.cpp)
add_executable(unit_test_scene     game/unit_test/unit_test_scene.cpp)
add_executable(unit_test_scriptvar game/unit_test/unit_test_scriptvar.cpp)
add_executable(unit_test_tilemap   game/unit_test/unit_test_tilemap.cpp)

# engine tests
add_executable(unit_test_settings engine/unit_test/unit_test_settings.cpp)
add_executable(unit_test_lua      engine/unit_test/unit_test_lua.cpp)
add_executable(unit_test_renderer engine/unit_test/unit_test_renderer.cpp)
if (MSVC)
    target_compile_options(unit_test_lua PRIVATE /bigobj)
endif()
target_link_libraries(unit_test_tilemap   GameLibTesting DataLib BaseLib)
target_link_libraries(unit_test_scriptvar GameLibTesting DataLib BaseLib)
target_link_libraries(unit_test_entity    GameLibTesting DataLib BaseLib)
target_link_libraries(unit_test_scene     GameLibTesting DataLib BaseLib)
target_link_libraries(unit_test_anim      GameLibTesting DataLib BaseLib ${CONAN_LIBS})
target_link_libraries(unit_test_tree      GameLibTesting DataLib BaseLib)
target_link_libraries(unit_test_lua       EngineLibTesting GameLibTesting DataLib UiLib AudioLib BaseLib wdk_system Lua ${CONAN_LIBS})
target_link_libraries(unit_test_settings  EngineLibTesting DataLib BaseLib)
target_link_libraries(unit_test_renderer  EngineLibTesting GameLibTesting GfxLib DataLib BaseLib wdk_system wdk_desktop_gl ${CONAN_LIBS})

target_include_directories(unit_test_tilemap   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(unit_test_scriptvar PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(unit_test_tree      PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(unit_test_anim      PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(unit_test_entity    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(unit_test_scene     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/game/unit_test")
target_include_directories(unit_test_settings  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/unit_test")
target_include_directories(unit_test_lua       PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/unit_test")
target_include_directories(unit_test_renderer  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/engine/unit_test")
add_test(NAME unit_test_tree      COMMAND unit_test_tree)
add_test(NAME unit_test_anim      COMMAND unit_test_anim)
add_test(NAME unit_test_settings  COMMAND unit_test_settings)
add_test(NAME unit_test_entity    COMMAND unit_test_entity)
add_test(NAME unit_test_scene     COMMAND unit_test_scene)
add_test(NAME unit_test_lua       COMMAND unit_test_lua)
add_test(NAME unit_test_rend      COMMAND unit_test_renderer)
add_test(NAME unit_test_scriptvar COMMAND unit_test_scriptvar)
add_test(NAME unit_test_tilemap   COMMAND unit_test_tilemap)

#UI kit tests
add_executable(unit_test_uikit
        uikit/unit_test/unit_test.cpp
        uikit/widget.cpp
        uikit/window.cpp)
target_include_directories(unit_test_uikit PRIVATE "${CMAKE_CURRENT_LIST_DIR}/uikit/unit_test")
target_link_libraries(unit_test_uikit DataLib BaseLib)
add_test(NAME unit_test_uikit COMMAND unit_test_uikit)

# audio tests
add_executable(unit_test_audio
        audio/unit_test/unit_test.cpp
        audio/pulseaudio.cpp
        audio/openal.cpp
        audio/waveout.cpp
        audio/player.cpp
        audio/proxy.cpp)
target_include_directories(unit_test_audio PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio/test")
target_link_libraries(unit_test_audio BaseLib)
if (UNIX)
    target_link_libraries(unit_test_audio pulse)
    target_link_libraries(unit_test_audio pthread)
#    target_link_libraries(unit_test_audio openal)
endif()
add_executable(unit_test_audio_graph
        audio/unit_test/unit_test_graph.cpp
        audio/loader.cpp
        audio/mpg123.cpp
        audio/sndfile.cpp
        audio/element.cpp
        audio/format.cpp
        audio/graph.cpp)
target_include_directories(unit_test_audio_graph PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio/test")
target_link_libraries(unit_test_audio_graph DataLib BaseLib samplerate sndfile ${CONAN_LIBS})

add_test(NAME unit_test_audio COMMAND unit_test_audio)
add_test(NAME unit_test_audio_graph COMMAND unit_test_audio_graph)

# base tests
add_executable(unit_test_grid    base/unit_test/unit_test_grid.cpp base/assert.cpp base/utility.cpp)
add_executable(unit_test_mem     base/unit_test/unit_test_memory.cpp base/assert.cpp)
add_executable(unit_test_math    base/unit_test/unit_test_math.cpp)
add_executable(unit_test_cmdline base/unit_test/unit_test_cmdline.cpp)
add_executable(unit_test_logging base/unit_test/unit_test_log.cpp base/logging.cpp base/assert.cpp)
add_executable(unit_test_base    base/unit_test/unit_test.cpp base/json.cpp base/utility.cpp base/trace.cpp base/assert.cpp)
target_include_directories(unit_test_grid    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test/")
target_include_directories(unit_test_mem     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test/")
target_include_directories(unit_test_base    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test/")
target_include_directories(unit_test_logging PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test/")
if (UNIX)
   target_link_libraries(unit_test_logging PRIVATE pthread)
endif()
target_include_directories(unit_test_math     PRIVATE "${CMAKE_CURRENT_LIST_DIR}/base/unit_test")
add_test(NAME unit_test_base    COMMAND unit_test_base)
add_test(NAME unit_test_grid    COMMAND unit_test_grid)
add_test(NAME unit_test_mem     COMMAND unit_test_mem)
add_test(NAME unit_test_math    COMMAND unit_test_math)
add_test(NAME unit_test_cmdline COMMAND unit_test_cmdline)
add_test(NAME unit_test_logging COMMAND unit_test_logging)

# gfx tests
add_executable(unit_test_bitmap   graphics/unit_test/unit_test_bitmap.cpp)
add_executable(unit_test_image    graphics/unit_test/unit_test_image.cpp)
add_executable(unit_test_graphics graphics/unit_test/unit_test.cpp)
add_executable(unit_test_device   graphics/unit_test/unit_test_device.cpp)
add_executable(unit_test_material graphics/unit_test/unit_test_material.cpp)
add_executable(unit_test_drawable graphics/unit_test/unit_test_drawable.cpp)
add_executable(unit_test_drawing graphics/unit_test/unit_test_drawing.cpp)

target_link_libraries(unit_test_image    GfxLibTesting DataLib BaseLib)
target_link_libraries(unit_test_graphics GfxLibTesting DataLib BaseLib)
target_link_libraries(unit_test_device   GfxLibTesting DataLib BaseLib wdk_system wdk_desktop_gl ${CONAN_LIBS})
target_link_libraries(unit_test_material GfxLibTesting DataLib BaseLib ${CONAN_LIBS})
target_link_libraries(unit_test_drawable GfxLibTesting DataLib BaseLib ${CONAN_LIBS})
target_link_libraries(unit_test_bitmap   GfxLibTesting DataLib BaseLib)
target_link_libraries(unit_test_drawing  GfxLibTesting DataLib BaseLib ${CONAN_LIBS})

add_test(NAME unit_test_drawable COMMAND unit_test_drawable)
add_test(NAME unit_test_drawing  COMMAND unit_test_drawing)
add_test(NAME unit_test_material COMMAND unit_test_material)
add_test(NAME unit_test_bitmap   COMMAND unit_test_bitmap)
add_test(NAME unit_test_image    COMMAND unit_test_image)
add_test(NAME unit_test_graphics COMMAND unit_test_graphics)
add_test(NAME unit_test_device   COMMAND unit_test_device)
target_include_directories(unit_test_bitmap   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_image    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_graphics PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_device   PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_material PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_drawing  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")
target_include_directories(unit_test_drawable PRIVATE "${CMAKE_CURRENT_LIST_DIR}/graphics/unit_test")

add_test(NAME gfx_test_msaa0  COMMAND graphics_test --test          --no-user WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")
add_test(NAME gfx_test_msaa4  COMMAND graphics_test --test --msaa4  --no-user WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")
add_test(NAME gfx_test_msaa8  COMMAND graphics_test --test --msaa8  --no-user WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")
add_test(NAME gfx_test_msaa16 COMMAND graphics_test --test --msaa16 --no-user WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/graphics/test/dist")

add_test(NAME audio_test_loop_mp3  COMMAND audio_test --loops 4 --file MP3/Alarm_Loop_00.mp3 WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/audio/test")
add_test(NAME audio_test_loop_ogg  COMMAND audio_test --loops 4 --file OGG/Alarm_Loop_00.ogg WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/audio/test")
add_test(NAME audio_test_loop_wav  COMMAND audio_test --loops 4 --file WAV/Alarm_Loop_00.wav WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/audio/test")
