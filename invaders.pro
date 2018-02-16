CONFIG += 64bit
CONFIG += qt
CONFIG += c++14
CONFIG += sse
CONFIG += ss2
CONFIG += debug_and_release

QT += opengl

SOURCES = game.cpp\
	gamewidget.cpp\
	level.cpp\
	main.cpp\
	qtmain_win.cpp\
	audio/device.cpp\
	audio/sample.cpp\
	audio/player.cpp\
	base/assert.cpp\
	base/format.cpp\
	base/logging.cpp\

HEADERS = game.h\
	gamewidget.h\
	level.h\
	audio/stream.h\
	audio/sample.h\
	audio/player.h\
	audio/device.h\
	base/assert.h\
	base/format.h\
	base/logging.h\

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

