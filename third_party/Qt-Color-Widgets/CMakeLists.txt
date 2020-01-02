#
# Copyright (C) 2013-2017 Mattia Basaglia
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
cmake_minimum_required(VERSION 2.6)
set(COLOR_WIDGETS_LIBRARY ColorWidgets-qt5)
set(PROJECT_VERSION "")
project(${COLOR_WIDGETS_LIBRARY} CXX)

# Qt
find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic -Wextra")

# C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# For some reason the above flags don't really work...
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(--std=c++${CMAKE_CXX_STANDARD} STD_CXX)
    if(STD_CXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++${CMAKE_CXX_STANDARD}")
    else()
        message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
    endif()
else()
    message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
endif()

# Sources
include_directories("${PROJECT_SOURCE_DIR}/include")

set(SOURCES
src/color_list_widget.cpp
src/gradient_slider.cpp
src/color_line_edit.cpp
src/swatch.cpp
src/bound_color_selector.cpp
src/color_palette_widget.cpp
src/color_palette_model.cpp
src/color_2d_slider.cpp
src/abstract_widget_list.cpp
src/color_delegate.cpp
src/color_widgets.qrc
src/color_utils.cpp
src/color_dialog.cpp
src/color_palette.cpp
src/color_selector.cpp
src/color_preview.cpp
src/alphaback.png
src/color_utils.hpp
src/hue_slider.cpp
src/color_wheel.cpp
src/color_names.cpp
)

set(HEADERS
include/color_palette_model.hpp
include/color_palette_widget.hpp
include/colorwidgets_global.hpp
include/color_wheel.hpp
include/color_line_edit.hpp
include/swatch.hpp
include/color_delegate.hpp
include/color_palette.hpp
include/hue_slider.hpp
include/abstract_widget_list.hpp
include/bound_color_selector.hpp
include/color_dialog.hpp
include/color_2d_slider.hpp
include/color_selector.hpp
include/color_list_widget.hpp
include/color_preview.hpp
include/gradient_slider.hpp
include/color_names.hpp
)

qt5_wrap_cpp(SOURCES ${HEADERS})
qt5_wrap_ui(SOURCES src/color_dialog.ui src/color_palette_widget.ui)
qt5_add_resources(SOURCES src/color_widgets.qrc)

# Library
add_library(${COLOR_WIDGETS_LIBRARY} ${SOURCES})
target_link_libraries(${COLOR_WIDGETS_LIBRARY} Qt5::Widgets)

add_subdirectory(gallery)

find_package(Qt5Designer)
if(Qt5Designer_FOUND)
    add_subdirectory(color_widgets_designer_plugin)
endif()
