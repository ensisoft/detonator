CONFIG += 64bit
CONFIG += qt
CONFIG += c++14
CONFIG += sse
CONFIG += ss2
CONFIG += debug_and_release

# this is the magic that makes having several source files with
# same filename to actually work and build properly.
CONFIG += object_parallel_to_source

QT += opengl
QT += core gui

SOURCES = game.cpp\
	gamewidget.cpp\
	level.cpp\
	main.cpp\
	audio/pulseaudio.cpp\
	audio/waveout.cpp\
	audio/sample.cpp\
	audio/player.cpp\
	base/assert.cpp\
	base/format.cpp\
	base/logging.cpp\
	graphics/fucking_graphics_device.cpp\
	graphics/painter.cpp\
	graphics/stb_image.c\

HEADERS = game.h\
	gamewidget.h\
	level.h\
	audio/stream.h\
	audio/sample.h\
	audio/player.h\
	audio/device.h\
	base/assert.h\
	base/bitflag.h\
	base/format.h\
	base/logging.h\
	graphics/painter.h\
	graphics/types.h\
	graphics/device.h\
	graphics/program.h\
	graphics/shader.h\
	graphics/geometry.h\
	graphics/material.h\
	graphics/drawable.h\
	graphics/texture.h\
	graphics/stb_image.h\

TARGET = invaders

unix:QMAKE_CXXFLAGS += -std=c++11
unix:QMAKE_CXXFLAGS += -Wno-unused-parameter
unix:LIBS += -lpulse
unix:LIBS += -lsndfile
unix:LIBS += -lncurses

win32:INCLUDEPATH += "c:/local/boost_1_61_0"
win32:INCLUDEPATH += "libsndfile"
win32:LIBS        += "libsndfile/libsndfile-1.lib"

CONFIG(debug, debug|release){
    DESTDIR = dist
} else {
    DESTDIR = dist
}

