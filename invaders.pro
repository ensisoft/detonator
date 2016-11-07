CONFIG += qt
CONFIG += c++11
CONFIG += sse
CONFIG += ss2
CONFIG += debug_and_release

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

unix:QMAKE_CXXFLAGS += -std=c++11
unix:QMAKE_CXXFLAGS += -Wno-unused-parameter
unix:LIBS += -lpulse
unix:LIBS += -lsndfile

win32:INCLUDEPATH += "c:/local/boost_1_61_0"

CONFIG(debug, debug|release){
    DESTDIR = dist
} else {
    DESTDIR = dist
}

