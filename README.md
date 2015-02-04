pinyin-invaders
===============

Evil chinese characters are invading. Your job is to kill them by shooting
them down with pinyin missiles and detonating bombs.

Running on Windows
-----------------------

1. install the vcredist_x66.exe package to install runtime libraries.
2. run invaders.exe
3. play


Running on Linux
-----------------------

1. run invaders.sh




Building from source for Linux
-------------------------------

1. Install and prepare boost.build

  ```
  $ wget http://sourceforge.net/projects/boost/files/boost/1.51.0/boost_1_51_0.zip/download boost_1_51_0.zip
  $ unzip boost_1_51_0.zip
  $ cd boost_1_51_0/tools/build/v2
  $ ./bootstrap.sh
  $ sudo ./b2 install 
  $ bjam --version
  Boost.build.2011.12-svn
  ```
  
2. Download and extract Qt everywhere and build it 

  ```
  $ tar -zxvvf qt-everywhere-opensource-src-4.8.2.tar.gz
  $ cd qt-everywhere-opensource-src-4.8.2
  $ ./configure --prefix=../qt-4.8.2 --no-qt3support --no-webkit
  $ make
  $ make install
  ```

3. build the game

  ```
  $ bjam
  ```

Building from source for Windows
---------------------------------

todo:
