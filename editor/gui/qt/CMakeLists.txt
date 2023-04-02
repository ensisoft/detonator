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
    ../collapsible_widget.h
    ../collapsible_widget.ui
    ../collapsible_widget.cpp)

target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../third_party")
target_include_directories(DetonatorQtDesignerPlugin PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/glm")
target_link_libraries(DetonatorQtDesignerPlugin Qt5::Designer Qt5::Svg Qt5::OpenGL)


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