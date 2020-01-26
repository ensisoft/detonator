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
  $ git clone https://github.com/ensisoft/pinyin-invaders
  $ git submodule update --init --recursive
  $ cd pinyin-invaders
  $ mkdir dist
  $ cd dist
  $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
  $ make
  $ cd ../game/dist/
  $ ./invaders
```

3. Debug the game / enable console logging
```
  $ git clone https://github.com/ensisoft/pinyin-invaders
  $ git submodule update --init --recursive
  $ cd pinyin-invaders
  $ mkdir dist_d
  $ cd dist_d
  $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
  $ make 
  $ cd ../game/dist/
  $ cgdb ../dist/invaders
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
  $ git clone https://github.com/ensisoft/pinyin-invaders
  $ git submodule update --init --recursive
  $ cd pinyin-invaders
  $ mkdir dist
  $ cmake -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release ..
  $ msbuild invaders.vcxproj /property:Configuration=Release /property:Platform=x64
  $ msbuild INSTALL.vcxproj
  $ cd ..\game\dist
  $ invaders.exe
```

5. Package dependencies for distribution
```
  $ cd dist
  $ windeployqt invaders.exe
```