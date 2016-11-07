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
  $ sudo apt-get install qt4-dev-tools
  $ sudo apt-get install libpulse-dev
  $ sudo apt-get install libsnd-dev

```

2. build the game

```
  $ qmake-qt4
  $ make
  $ dist/invaders 
```


Building from source for Windows
---------------------------------

The code is written against Qt4. The latest Qt4.x prebuilt binaries for Windows target 
Microsoft Visual Studio 2010. However the code uses some C++11 features  so a newer
Visual Studio Version is needed.

1. Install Microsoft Visual Studio 2015 Community
https://www.visualstudio.com/downloads/

2. Install prebuilt Qt 
https://download.qt.io/archive/qt/4.8/4.8.4/qt-win-opensource-4.8.4-vs2010.exe

3. Install prebuilt Boost
https://sourceforge.net/projects/boost/files/boost-binaries/1.61.0/boost_1_61_0-msvc-14.0-64.exe/download

4. Open command prompt
```
  $ set PATH=%PATH%;c:\Qt\4.8.4\bin
  $ cd pinyin-invaders
  $ qmake -tp vc invaders.pro
``` 

5. Open the invaders.vcxproj
Since your Visual Studio version is newer you'll need to retarget the solution for the newer toolchain.
Right click on the solution in the solution browser and select "Retarget solution". 

Select Release config and hit build. Debug build by default builds against Qt's debug libraries which
require msvs2010 debug runtime which you don't necessarily have.

6. Run the game 
```
  $ set PATH=%PATH%;c\Qt\4.8.4\bin
  $ cd pinyin-invaders
  $ dist\invaders.exe
```
