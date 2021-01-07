Ensisoft Gamestudio
===================

A 2D game creation tool. Currently only catering towards dealing with the 2D graphics of things.

Currently supported major features.
* Qt5 based editor for building graphics assets
  * Separate workspaces for project based work
  * WYSIWYG editing for animations, materials, particle systems and custom polygon shapes
  * Process isolated editor game playback
* Text rendering
* Various primitive shapes rendering
* Material system
* Particle system
* Animation system
* Window integration (https://github.com/ensisoft/wdk)
* Minimalistic audio engine
    
Planned major features:
* Scene builder 
* HUD builder
* Menu builder 
* Scripting support (most likely Lua)
* Physics support (most likely Box2D)
* Android support 

![Screenshot](https://raw.githubusercontent.com/ensisoft/gamestudio/master/screens/editor-animation.png "Animation editor")
![Screenshot](https://raw.githubusercontent.com/ensisoft/gamestudio/master/screens/editor-material.png "Material editor")
![Screenshot](https://raw.githubusercontent.com/ensisoft/gamestudio/master/screens/editor-particle.png "Particle editor")

Build Instructions
==================

Penguin Juice (Linux)
------------------------------

- Install the dev packages.
  (for Ubuntu based systems)
```
  $ sudo apt-get install libboost-dev
  $ sudo apt-get install qtbase5-dev
  $ sudo apt-get install libpulse-dev
```
- Install Conan build system.  
https://docs.conan.io/en/latest/installation.html
   
- Build the project in RELEASE mode
```
  $ git clone https://github.com/ensisoft/gamestudio
  $ git submodule update --init --recursive
  $ cd gamestudio
  $ mkdir build
  $ cd build
  $ conan install .. --build missing
  $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
  $ make -j16 install
  $ ctest -j16
```

- Build the project in DEBUG mode
```
  $ git clone https://github.com/ensisoft/gamestudio
  $ git submodule update --init --recursive
  $ cd gamestudio
  $ mkdir build_d
  $ cd build_d
  $ conan install .. --build missing
  $ cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
  $ make -j8 install
  $ ctest -j16
```

Boring But Stable (Windows)
---------------------------------

These build instructions are for MSVS 2019 Community Edition and for 64bit build.

- Install Microsoft Visual Studio 2019 Community  
https://www.visualstudio.com/downloads/

- Install prebuilt Qt 5.15.2
http://download.qt.io/official_releases/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.zip

- Install prebuilt Boost 1.72  
https://sourceforge.net/projects/boost/files/boost-binaries/1.72.0_b1/

- Install conan package manager  
https://docs.conan.io/en/latest/installation.html

- Open "Developer Command Prompt for VS 2019"

```
  $ git clone https://github.com/ensisoft/gamestudio
  $ git submodule update --init --recursive
  $ cd gamestudio
  $ mkdir dist
  $ cd dist
  $ conan install .. --build missing
  $ cmake --build   . --config Release
  $ cmake --install . --config Release
```

The Boring Documentation
=======================

Workflow and getting started
----------------------------
Going from a git clone to a running game is known as the "build workflow". This involves various steps 
involving the source tree of the engine, the source tree of the game, the runtime assets provided with the
engine and the runtime assets that are part of the game and this will involve several build and packaging steps.

! The expectation is that the game and the engine evolve in a lockstep and therefore both are part of the same
code repository. This is easiest way to manage the engine/game dependency.

! Currently only C++ based games are supported. The game will be built into a shared library (.so or .dll) and
  will contain all the relevant code (the game play code, graphics, audio etc). A launcher application (GameMain)
  can then be used to launch the game. The launcher application will:
  1. Read a config.json file and create window and rendering context for your game
  2. Load the game library
  3. Create an instance of game::App object by calling the entry point in the game lib
  4. Enter a game loop and invoke virtual functions on the game::App object in order to:
     1. Update the game
     2. Tick the game
     3. Render the game
     4. Handle input events (mouse, keyboard)
     5. Handle platform events (rendering surface resized etc.)
     6. Handle application requests

From source code to a running game:
1. Build the whole project as outlined in the **Build Instructions** previously. (Or at minimum build the 
   Editor, EditorGameHost, GameMain and *your* game library, if you know what you're doing). You will 
   also need to *install* the build targets. Installing not only copies the binaries into the right place
   but also copies other assets such as GLSL shader files and this is very important.
2. After building launch the Editor from the editor/dist folder.
3. *In the Editor:* Open your game's workspace folder. This is the folder that contains the workspace.json and content.json files.
   In GameStudio this is essentially your "project" (It's simply a folder with those two specific files in it).
4. *In the Editor:* Package your content. (Workspace|Package). Select the assets you want to package (most likely everything).
   When the packaging process is complete your selected output directory will contain the following:
   1. The game resources copied over, i.e. shaders, textures, font files etc.
   2. A content.json file that contains the resource descriptions for your assets that you've built in the editor
   3. A config.json file that contains the settings for the GameMain to launch the game
   4. An executable by the name of your game. This is the renamed copy of GameMain and is used to launch your game
5. After packaging launch your game by running the "yourgame.exe" in the package output directory.   
   
Dealing with Assets
-------------------   
todo:   
   
Architecture
------------
![Archicture diagram](https://raw.githubusercontent.com/ensisoft/gamestudio/master/docu/stack.png "Stack")   

Games
=====

pinyin-invaders
---------------

Evil chinese characters are invading. Your job is to kill them by shooting
them down with pinyin missiles and detonating bombs. Learn Mandarin Chinese characters by playing this simple game that asks you to memorize characters.

![Screenshot](https://raw.githubusercontent.com/ensisoft/gamestudio/master/screens/invaders-menu.png "Main menu")

![Screenshot](https://raw.githubusercontent.com/ensisoft/gamestudio/master/screens/invaders-game.png "pinyin-invaders are attacking!")













