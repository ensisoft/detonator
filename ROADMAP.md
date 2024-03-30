DETONATOR 2D
=====================================

This is the non exhaustive list of features and ideas that I have in mind
for this game engine and that I'm planning to develop. Not everything on this
list is going to happen in the exact way it's currently written, some features
are more just ideas right now.

Development Roadmap
-------------------------------------

#### Functional Features

* Tilemap features
    * Height layer for tile height adjustment [#198](i198)
    * Save layer for saving data that can be persisted between game runs. [#199](i199)
    * Lua APIs for data access etc.
    * Logical data layer related algorithms such as path finding
    * Compression etc. performance improvements
    * Chunked data loading
    * ~~Isometric tilemap integration with scene+entity system~~  DONE

* Game play system
    * Game state loading and saving [#200](i200)
    * Some more entity components:
        * Path finding component
        * Pixel collider
        * Audio emitter
        * User input receiver

* Platform support
    * HighDPI content scaling (mobile device support) [#84][i84]
    * Content rotation in landscape mode [#165](i165)
    * Touch screen input [#164](i164)
    * OpenGL ES3 backend and WebGL2 support [#55][i55]
        * Instanced rendering, transform feedback, compute shaders, MSAA FBO
        * Currently ES3 Context is supported but no actual ES3 features are in use yet

* Rendering features
  * Fabricate depth values and use depth buffer for layering  
  * Lights and shadows
  * Fluid dynamics in the particle simulations [#52][i52] [#53][i53]
  * Partial 3D object support for specific objects [#201][i201]
      * Think objects such as coins, diamonds, player's ship etc.
  * Some post processing effects
      * ~~Bloom~~ DONE (with issues since RGBA8 render target) 
      * Motion blur

#### Performance Features
* Acceleration structures for rendering and physics
  * Improve the renderer data structures, only keep around what's in the current viewport. 
* Plenty of asset baking features
    * For example audio pre-render when possible

#### Minor Features
* See issues for more details

[i52]:  https://github.com/ensisoft/detonator/issues/52
[i53]:  https://github.com/ensisoft/detonator/issues/53
[i55]:  https://github.com/ensisoft/detonator/issues/55
[i84]:  https://github.com/ensisoft/detonator/issues/84
[i165]: https://github.com/ensisoft/detonator/issues/165
[i164]: https://github.com/ensisoft/detonator/issues/164
[i198]: https://github.com/ensisoft/detonator/issues/198
[i199]: https://github.com/ensisoft/detonator/issues/199
[i200]: https://github.com/ensisoft/detonator/issues/200
[i201]: https://github.com/ensisoft/detonator/issues/201