# Copyright (C) 2013-2017 Mattia Basaglia
#
#
# This software is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Color Widgets.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.6)
set(COLOR_WIDGETS_PLUGIN ColorWidgetsPlugin)
set(PROJECT_VERSION "")
project(${COLOR_WIDGETS_PLUGIN} CXX)

# Qt
find_package(Qt5Designer REQUIRED)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${Qt5Designer_INCLUDE_DIRS})

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic -Werror")

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
set(SOURCES
color_preview_plugin.cpp
color_wheel_plugin.cpp
color_widget_plugin_collection.cpp
gradient_slider_plugin.cpp
hue_slider_plugin.cpp
color_selector_plugin.cpp
color_list_plugin.cpp
swatch_plugin.cpp
color_palette_widget_plugin.cpp
color_2d_slider_plugin.cpp
color_line_edit_plugin.cpp
# add new sources above this line
)

set(HEADERS
color_preview_plugin.hpp
color_wheel_plugin.hpp
color_widget_plugin_collection.hpp
gradient_slider_plugin.hpp
hue_slider_plugin.hpp
color_selector_plugin.hpp
color_list_plugin.hpp
swatch_plugin.hpp
color_palette_widget_plugin.hpp
color_2d_slider_plugin.hpp
color_line_edit_plugin.hpp
# add new headers above this line
)

qt5_wrap_cpp(SOURCES ${HEADERS})

# Library
add_definitions(-DQTCOLORWIDGETS_LIBRARY)
add_library(${COLOR_WIDGETS_PLUGIN} MODULE EXCLUDE_FROM_ALL ${SOURCES})
target_link_libraries(${COLOR_WIDGETS_PLUGIN} Qt5::Designer)
target_link_libraries(${COLOR_WIDGETS_PLUGIN} ${COLOR_WIDGETS_LIBRARY})

# install
get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake LOCATION)
execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_INSTALL_PLUGINS OUTPUT_STRIP_TRAILING_WHITESPACE)
# install(TARGETS ${COLOR_WIDGETS_PLUGIN} DESTINATION ${QT_INSTALL_PLUGINS}/designer OPTIONAL)

execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_LIBS
    OUTPUT_VARIABLE QT_INSTALL_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
# install(TARGETS ${COLOR_WIDGETS_PLUGIN} DESTINATION ${QT_INSTALL_LIBS}/qtcreator/plugins OPTIONAL)

add_custom_target(${COLOR_WIDGETS_PLUGIN}_install
    COMMAND cp $<TARGET_FILE:${COLOR_WIDGETS_PLUGIN}> ${QT_INSTALL_PLUGINS}/designer
    COMMAND cp $<TARGET_FILE:${COLOR_WIDGETS_PLUGIN}> ${QT_INSTALL_LIBS}/qtcreator/plugins
    DEPENDS ${COLOR_WIDGETS_PLUGIN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
