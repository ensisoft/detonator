Ensisoft Gamestudio
===================

A 2D game engine and editor.

![Screenshot](https://raw.githubusercontent.com/ensisoft/gamestudio/master/screens/bandit-play.gif "Bandit demo")

Currently supported major features.
* Qt5 based editor for building graphics assets
  * Separate workspaces for project based work
  * WYSIWYG editing for animations, materials, particle systems and custom polygon shapes
  * Process isolated editor game playback
* Text rendering
* Various primitive shapes rendering
* Material system
* Particle system
* Animated entity system
* Window integration (https://github.com/ensisoft/wdk)
* Minimalistic audio engine
* Lua based scripting support including built-in code editor
* Scene builder
* UI builder
* Box2D based physics support
* Win32 / Linux support (both for Editor and as game build target)
* Demo content 
    
Planned major features not yet implemented:
* Android and/or HTML5 build target support (TBD)

Planned minor features not yet implemented:
* Audio mixer and engine side audio integration
* Help/API documentation for the game Lua API.
* Acceleration structures / scene queries / ray casting
* Several other functional and performance improvements
* Facilities for loading/saving game data/state from Lua

![Screenshot](screens/editor-animation.png "Animation editor")
Create your animated game play characters in the entity editor. Each entity can contain an arbitrary render tree
of nodes with various attachments for physics, rendering and text display. Animation tracks allow changing the properties
of the nodes and transforming the tree itself over time. Each entity type can then be associated with a Lua script where
you can write your entity specific game play code. 

![Screenshot](screens/editor-material.png "Material editor")
Create materials using the material editor by setting some properties for the provided default material shaders.
Currently supports sprite animations, textures (including text and noise), gradient and color fills out of box. 
Custom shaders can be used too.   

![Screenshot](screens/editor-scene.png "Scene builder")
Create the actual game scene using the scene editor. The entities you create in the entity editor are available here
for placing in the scene. Viewport visualization will quickly show you how much of the game world will be seen when
the game plays.

![Screenshot](screens/demo-bandit.png "Editor game play window")
During the development the game is available for play in the editor. It's possible to do live edits to the 
game content in the editor and see the changes take place in the play window.

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
  $ cd gamestudio
  $ git submodule update --init --recursive
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
  $ cd gamestudio
  $ git submodule update --init --recursive
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
  $ cd gamestudio
  $ git submodule update --init --recursive
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
engine and the runtime assets that are part of the game. Overall this will involve several build and packaging steps.

Currently only Lua based games are supported. There's a provided game engine that is be built into a shared library 
and which contains all the relevant functionality for running a game. That is it will take care of rendering the scene,
ticking the physics engine, handling the UI input as well as invoking the Lua scripts in order to run *your* game code. 
It's possible to also not use this provided engine but write a completely different engine. In this case the interface
to implement is the "App" interface in engine/main/interface.h. 

From source code to a running game:
1. Build the whole project as outlined in the **Build Instructions** previously. 
   You will also need to *install* the build targets. Installing not only copies the binaries into the right place
   but also copies other assets such as GLSL shader files. Without the install step things will not work correctly!
2. After building launch the Editor from the editor/dist folder.
3. *In the Editor:* Open your game's workspace folder. This is the folder that contains the workspace.json and content.json files.
   In Gamestudio this is essentially your "project" (It's simply a folder with those two specific files in it).
4. *In the Editor:* Package your content. (Workspace|Package). Select the assets you want to package (most likely everything).
   When the packaging process is complete your selected output directory will contain the following:
   1. The game resources copied over, i.e. shaders, textures, font files etc.
   2. A content.json file that contains the resource descriptions for your assets that you've built in the editor
   3. A config.json file that contains the settings for the GameMain to launch the game
   4. A "GameMain" executable and engine library.  These are the default game studio binaries for running your 
      game content after the project has been packaged. 
5. After packaging launch your game by running the GameMain executable in the package output directory.
   
Dealing with File Resources
---------------------------   
When building game content it's normal to use resources created in other applications and stored somewhere else on your 
computer. Examples of these resources are font files (.ttf, .otf), texture files (.png, .bmp, .jpe?g) and shader (.glsl)
files. For example you might use some image editor to create your textures or you might have downloaded them from some
site such as http://www.opengameart.org. Regardless these are normally found somewhere on your computer's hard drive.  
   
Unlike some other tools Gamestudio does *not* have an import step that would copy the files into your project tree. 
Rather it allows you to use these files from whichever location they have. In other words Gamestudio only stores the path
to the resources in its own project files. When the project/workspace content is packaged the resources are all gathered
and packaged and then finally copied into the designated output folder.

What does this mean to you?

1. If you're only working on your game yourself on a single machine you can place your game resources wherever you wish
   and any time you update your resource files the changes are reflected in your game project without any need to "reimport".
   (Just reload your textures and/or shaders).
2. However if you want to be able to share your game project with other people you must store your resource files in a
   location that is universally available on every single machine.
   
The simplest way is to simply place your resource files under your workspace directory. For example:

mygame/textures/player/texture0.png
mygame/textures/player/texture1.png
...
mygame/content.json
mygame/workspace.json
   
When a resource that is stored under the workspace is added to the project/workspace the path is always stored relative
to the workspace itself. This makes the whole workspace self contained and portable across machines.

You can then additionally add your whole game project directory into some version control tool such as Git. 
   
System Architecture
-------------------
![Archicture diagram](docu/stack.png "Stack")

An overview of the runtime architecture:

1. There's a standard game engine that is built into a shared library called GameEngine.dll or libGameEngine.so.
2. A launcher application called GameMain will read a config.json file. The JSON file contains information about 
   windowing, context creation etc. 
   1. The launcher application will load the engine library and create an Application object instance. 
   2. The launcher application will create the native window system window and also create the Open GL rendering context 
      which it will then give to the app instance.
3. The launcher application will then enter the top-level game loop. Inside the loop it will: 
   1. Process any window system window events to handle pending keyboard/mouse etc. events and pass them to the app as needed.
   2. Handle events coming from the app instance. These could be for example requests to toggle full screen mode.
   3. Accumulate and track time, call functions to Update, Tick and Draw the app instance.

Inside the game engine the following will take place.
1. The various subsystems are updated and ticked. These include physics, audio, scripting etc.
2. The renderer is used to draw the current background and foreground scenes. 
3. The engine will handle the input events (mouse, keyboard) coming from the host process and choose the appropriate action.
   For example If a UI is being shown the input is passed to the UI subsystem. The inputs are then also passed to the 
   game scripting system and ultimately to your game's Lua scripts. 
4. The engine will handle the incoming events/requests from the game itself. For example the game might request a scene
   to be loaded or the game to be paused. 
