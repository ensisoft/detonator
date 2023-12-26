DETONATOR 2D 💥💣
===================
An OpenGL ES based 2D game engine and editor for Linux, Windows and HTML5. Designed for simple single player games such
as puzzle games, platformers, side scrollers and tile based real time strategy and tactics.  
🍄🧩🗺️

[![SHMUP DEMO](screens/blast-thumbnail.png)](https://www.ensisoft.com/github/blast.mp4)
  

### CURRENT TOP FEATURES !

* Windows, Linux and HTML5/WASM support
* Qt5 based WYSIWYG editor
* Text rendering (vector and bitmap)
* Various primitive shapes, custom polygon shapes
* Material system with built-in materials and with custom shaders
* Particle system with projectile and linear motion
* Entity system with animation tracks
* Audio engine with approx. dozen audio elements
* Lua based scripting for entities, scenes and UIs
* Built-in Lua script editor with code formatting, API help and code completion
* Scene builder
* In game UI system
  * Animation ready through simple CSS inspired keyframe declarations
  * Styling support through JSON style files *and* material system integration
  * Virtual key support and mouse input support
  * Scripting support for integrating with the game
* Tilemap builder for tile based worlds
  * Multiple render and data layers
  * Isometric (dimetric) and axis aligned top down support
  * Combines with scene and its entities!
* Physics engine based on Box2D
* Demo content and starter content
* Game content packaging for native and HTML5/WASM (with Emscripten)
* Resource archives, export and import between projects (in zip)
* Tilemap importer, several handy dialogs for materials, fonts, colors etc.
* Several other tools such as:
  * Image packer (for packing textures manually when needed)
  * Bitmap font mapper (map glyps to characters and vice versa)
  * SVG viewer and PNG exporter
  * VCS (Git) integration for syncing project changes to Git

<br>



### DOWNLOAD LATEST WINDOWS BINARY

https://github.com/ensisoft/detonator/releases/tag/rel-4

### ROADMAP

<details><summary>Click here for roadmap</summary> 

#### Functional Features

* Tilemap features
  * Height layer for tile height adjustment
  * Lua APIs for data access etc.
  * Logical data layer related algorithms such as path finding
  * Compression etc. performance improvements
  * Chunked data loading
  * ~~Isometric tilemap integration with scene+entity system~~  DONE

* Game play system
  * Game state loading and saving

* Platform support
  * HighDPI content scaling (mobile device support) [#84][i84]
  * Content rotation in landscape mode [#165](i165)
  * Touch screen input [#164](i164)
  * OpenGL ES3 backend and WebGL2 support [#55][i55]
    * Instanced rendering, transform feedback, compute shaders, MSAA FBO

* Rendering features
  * Lights and shadows
  * Fluid dynamics in the particle simulations
  * Partial 3D object support for specific objects
    * Think objects such as coins, diamonds, player's ship etc.
  * Some post processing effects
    * Bloom DONE but sucks because RGBA render target, no floating point FBO :(

#### Performance Features
* Acceleration structures for rendering and physics
* Plenty of asset baking features
  * For example audio pre-render when possible

#### Minor Features
* See issues for more details

</details>

[i55]:  https://github.com/ensisoft/detonator/issues/55
[i84]:  https://github.com/ensisoft/detonator/issues/84
[i165]: https://github.com/ensisoft/detonator/issues/165
[i164]: https://github.com/ensisoft/detonator/issues/164

### SCREENSHOTS


![Screenshot](screens/editor-tilemap.png "Map editor")
Create tile based maps using the tile editor. The map supports multiple layers and both isometric and axis aligned perspective.
The map can then be combined with the scene and the scene based entities in order to produce the final game world.

![Screenshot](screens/editor-animation.png "Entity editor")
Create animated game play characters in the entity editor. Each entity can contain an arbitrary render tree
of nodes with various attachments for physics, rendering, text display etc. The entity system supports scriptable
animation state graph as well as animation tracks for managing animation and entity state over time.
Each entity type can then be associated with a Lua script where you can write your entity specific game play code.

![Screenshot](screens/editor-material.png "Material editor")
Create materials using the material editor by adjusting properties for the provided default material shaders or
create your own materials with custom shaders! Currently supports sprite animations, textures (including text and noise), 
gradient and color fills out of box.

![Screenshot](screens/editor-scene.png "Scene editor")
Create the game play scenes using the scene editor. The entities you create in the entity editor are available here
for placing in the scene. Viewport visualization will quickly show you how much of the game world will be seen when
the game plays.

![Screenshot](screens/editor-ui.png "UI editor")
Create the game's UI in the UI editor. The UI and the widgets can be styled using a JSON based style file and then individual widgets
can have their style properties fine-tuned in the editor. The style system integrates with the editor's material system too!

![Screenshot](screens/editor-audio.png "Audio graph editor")
Create audio graphs using the audio editor. Each audio graph can have a number of elements added to it. The graph then
specifies the flow of audio PCM data from source elements to processing elements to finally to the graph output. 
Currently, supported audio backends are Waveout on Windows, Pulseaudio on Linux and OpenAL on HTML5/WASM. 
Supported formats are wav, mp3, ogg and flac.

![Screenshot](screens/editor-script.png "Script editor")
Use the built-in code editor to write the Lua scripts for the entities, scenes, game or UI. The editor has a built-in
help system for accessing the engine side Lua API documentation as well as automatic Lua code formatting, linting and
a code completion system! 

![Screenshot](screens/editor-particle.png "Particle editor")
Create different types of particle effects in the particle editor by conveniently adjusting several sliders 
and knobs that control the particle effect. 

## DEV ZONE
### [Build Instructions](BUILDING.md)
### [System Architecture](ARCHITECTURE.md)
### [Design Document](DESIGN.md)
### [Tracing & Profiling](PROFILING.md)

### Engine Subsystems

* [Audio](audio/README.md "Audio readme") 🎼
* [Graphics](graphics/README.md "Graphics readme") (todo)
* [Game](game/README.md "Game readme") (todo) 👾
* [Engine](engine/README.md "Engine readme") (todo)
* [UI](uikit/README.md "UIKit readme")
* [WDK](https://github.com/ensisoft/wdk/blob/master/README.md "WDK readme")


## THANKS

This project would not be possible without the following 🙏 
* Qt, GLM, Freetype, Harfbuzz, Lua, sol3, STB, nlohmann/json, mpg123, libsndfile, Box2D, Emscripten and many others!
* Royalty free art from [https://opengameart.com](https://opengameart.com "https://opengameart.com")