pinyin-invaders
===============

Evil chinese characters are invading. Your job is to kill them by shooting
them down with pinyin missiles and detonating bombs.

![Screenshot](https://raw.githubusercontent.com/ensisoft/pinyin-invaders/master/screens/menu.png "Main menu")

![Screenshot](https://raw.githubusercontent.com/ensisoft/pinyin-invaders/master/screens/invaders.png "pinyin-invaders are attacking!")

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

2. Build the game

```
  $ qmake
  $ make
  $ dist/invaders 
```

3. Debug the game / enable console logging
```
  $ qmake
  $ make debug
  $ cgdb dist/invaders
```


Building from source for Windows
---------------------------------

These build instructions are for MSVS 2015 Community Edition and for 64bit build.

1. Install Microsoft Visual Studio 2015 Community
https://www.visualstudio.com/downloads/

2. Install prebuilt Qt 
http://download.qt.io/official_releases/qt/5.11/5.11.3/qt-opensource-windows-x86-5.11.3.exe


3. Install prebuilt Boost
https://sourceforge.net/projects/boost/files/boost-binaries/1.61.0/boost_1_61_0-msvc-14.0-64.exe/download

4. Open "VS2015 x64 Native Tools" command prompt
```
  $ set PATH=%PATH%;c:\Qt\Qt5.11.3\5.11.3\msvc2015_64\bin
  $ cd pinyin-invaders
  $ qmake -tp vc invaders.pro
  $ msbuild invaders.vcxproj /property:Configuration=Release /property:Platform=x64
  $ dist\invaders.exe
``` 

