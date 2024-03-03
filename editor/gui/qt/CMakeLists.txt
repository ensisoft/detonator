cmake_minimum_required(VERSION 3.2)

project(DetonatorQtDesignerPlugin)

if (WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/msvc2019_64")
endif()

# Qt
find_package(Qt5Designer REQUIRED)

# needed because the widget source pulls in app/format which refers to
# some Qt types.
find_package(Qt5 REQUIRED COMPONENTS Core Network Svg OpenGL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${Qt5Designer_INCLUDE_DIRS})


# see this bug report about C++14 and Qt
# https://bugreports.qt.io/browse/QTBUG-53002
set(CMAKE_CXX_STANDARD 17)

add_library(QtColorWidgets
        ../../../third_party/Qt-Color-Widgets/src/abstract_widget_list.cpp
        ../../../third_party/Qt-Color-Widgets/src/bound_color_selector.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_2d_slider.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_delegate.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_dialog.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_dialog.ui
        ../../../third_party/Qt-Color-Widgets/src/color_line_edit.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_list_widget.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_names.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_palette.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_palette_model.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_palette_widget.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_palette_widget.ui
        ../../../third_party/Qt-Color-Widgets/src/color_preview.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_selector.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_utils.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_wheel.cpp
        ../../../third_party/Qt-Color-Widgets/src/color_widgets.qrc
        ../../../third_party/Qt-Color-Widgets/src/gradient_slider.cpp
        ../../../third_party/Qt-Color-Widgets/src/hue_slider.cpp
        ../../../third_party/Qt-Color-Widgets/src/swatch.cpp
        ../../../third_party/Qt-Color-Widgets/include/abstract_widget_list.hpp
        ../../../third_party/Qt-Color-Widgets/include/bound_color_selector.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_2d_slider.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_delegate.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_dialog.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_line_edit.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_list_widget.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_names.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_palette.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_palette_model.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_palette_widget.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_preview.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_selector.hpp
        ../../../third_party/Qt-Color-Widgets/include/color_wheel.hpp
        ../../../third_party/Qt-Color-Widgets/include/colorwidgets_global.hpp
        ../../../third_party/Qt-Color-Widgets/include/gradient_slider.hpp
        ../../../third_party/Qt-Color-Widgets/include/hue_slider.hpp
        ../../../third_party/Qt-Color-Widgets/include/swatch.hpp)
target_include_directories(QtColorWidgets PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/Qt-Color-Widgets/include")
target_compile_definitions(QtColorWidgets PRIVATE "QTCOLORWIDGETS_LIBRARY")
target_link_libraries(QtColorWidgets Qt5::Widgets Qt5::Core Qt5::CorePrivate)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # suppress some noise coming from Qt (Qt-Color-Widgets)
    target_compile_options(QtColorWidgets PRIVATE -Wno-deprecated-declarations)
endif()


add_library(DetonatorQtDesignerPlugin SHARED
    plugin.cpp
    plugin.h
    ../svg/svgview.cpp
    ../doubleslider.cpp
    ../spinboxwidget.cpp
    ../spinboxwidget.h
    ../doublespinbox.ui
    ../rangewidget.h
    ../rangewidget.cpp
    ../treewidget.h
    ../treewidget.cpp
    ../curvewidget.cpp
    ../curvewidget.h
    ../collapsible_widget.h
    ../collapsible_widget.ui
    ../collapsible_widget.cpp
    ../widgetstylewidget.ui
    ../../resource/resource.qrc)


target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../third_party")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/glm")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/Qt-Color-Widgets/include")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../wdk")
target_link_libraries(DetonatorQtDesignerPlugin Qt5::Designer Qt5::Svg Qt5::OpenGL)
target_link_libraries(DetonatorQtDesignerPlugin QtColorWidgets)
target_compile_definitions(DetonatorQtDesignerPlugin PRIVATE DETONATOR_DESIGNER_PLUGIN_BUILD)


# install
get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake LOCATION)
execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_INSTALL_PLUGINS OUTPUT_STRIP_TRAILING_WHITESPACE)
install(TARGETS DetonatorQtDesignerPlugin DESTINATION ${QT_INSTALL_PLUGINS}/designer OPTIONAL)

execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_LIBS
    OUTPUT_VARIABLE QT_INSTALL_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
install(TARGETS DetonatorQtDesignerPlugin DESTINATION ${QT_INSTALL_LIBS}/qtcreator/plugins OPTIONAL)


#add_custom_target(${COLOR_WIDGETS_PLUGIN}_install
#    COMMAND cp $<TARGET_FILE:${COLOR_WIDGETS_PLUGIN}> ${QT_INSTALL_PLUGINS}/designer
#    COMMAND cp $<TARGET_FILE:${COLOR_WIDGETS_PLUGIN}> ${QT_INSTALL_LIBS}/qtcreator/plugins
#    DEPENDS ${COLOR_WIDGETS_PLUGIN}
#    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})