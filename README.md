Ensisoft Gamestudio
===================

A 2D game creation tool. Currently only catering towards dealing with the 2D graphics of things.

Currently supported features.
* Qt5 based editor for building graphics assets
* Text rendering
* Various primitive shapes rendering
* Simple material system
* Particle system rendering
* Transformation hiearchies
* Window integration (https://github.com/ensisoft/wdk)
* Minimalistic audio engine


Building from source for Linux
-------------------------------

1. Install the dev packages.

  (for Ubuntu based systems)
```
  $ sudo apt-get install libboost-dev
  $ sudo apt-get install qtbase5-dev
  $ sudo apt-get install libpulse-dev
  $ sudo apt-get install libsnd-dev

```

2. Build the project RELEASE

```
  $ git clone https://github.com/ensisoft/gamestudio
  $ git submodule update --init --recursive
  $ cd gamestudio
  $ mkdir build
  $ cd build
  $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
  $ make -j8 install
```

3. Debug the project DEBUG
```
  $ git clone https://github.com/ensisoft/gamestudio
  $ git submodule update --init --recursive
  $ cd gamestudio
  $ mkdir build_d
  $ cd build_d
  $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
  $ make -j8 install
```

Building from source for Windows
---------------------------------

These build instructions are for MSVS 2019 Community Edition and for 64bit build.

1. Install Microsoft Visual Studio 2019 Community
https://www.visualstudio.com/downloads/

2. Install prebuilt Qt 5.12.6

http://download.qt.io/official_releases/qt/5.12/5.12.6/qt-opensource-windows-x86-5.12.6.exe

3. Install prebuilt Boost 1.72
https://sourceforge.net/projects/boost/files/boost-binaries/1.72.0_b1/

4. Open "Developer Command Prompt for VS 2019"
```
  $ git clone https://github.com/ensisoft/gamestudio
  $ git submodule update --init --recursive
  $ cd gamestudio
  $ mkdir dist
  $ cmake -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release ..
  $ msbuild invaders.vcxproj /property:Configuration=Release /property:Platform=x64
  $ msbuild INSTALL.vcxproj
```

Games
-----


pinyin-invaders
===============

Evil chinese characters are invading. Your job is to kill them by shooting
them down with pinyin missiles and detonating bombs. Learn Mandarin Chinese characters by playing this simple game that asks you to memorize characters.

![Screenshot](https://raw.githubusercontent.com/ensisoft/pinyin-invaders/master/screens/invaders-menu.png "Main menu")

![Screenshot](https://raw.githubusercontent.com/ensisoft/pinyin-invaders/master/screens/invaders-game.png "pinyin-invaders are attacking!")

Running on Windows
-----------------------

1. Unzip invaders.zip
1. install the vcredist_x86.exe package to install runtime libraries.
2. run invaders.exe
3. play


Running on Linux
-----------------------

1. Extract the invaders.tar.gz
2. run dist/invaders.sh












