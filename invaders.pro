CONFIG += qt
CONFIG += c++11
CONFIG += sse
CONFIG += ss2
CONFIG += debug_and_release

QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += -Wno-unused-parameter


SOURCES = audio.cpp\
	game.cpp\
	gamewidget.cpp\
	level.cpp\
	main.cpp\
	mainwindow.cpp\
	qtmain_win.cpp

HEADERS = audio.h\
	game.h\
	gamewidget.h\
	level.h\
	mainwindow.h\
	
FORMS = mainwindow.ui \

TARGET = invaders

unix:LIBS += -lpulse
unix:LIBS += -lsndfile

CONFIG(debug, debug|release){
    DESTDIR = dist
} else {
    DESTDIR = dist
}

