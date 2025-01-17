DETONATOR 2D 💥💣
===============

The Game Engine 🚂
----------------

Engine is the top level generic game engine that combines
various components and subsystems into a single component
simply called the "engine".

This includes the following engine subsystems<br>
* Rendering system
* Physics engine
* Audio engine
* Scripting system
* Gameplay system
* User interface


### Technical Notes

The engine is built into a shared library and implements
the main engine interface that the host application then uses to
interact with it.

Under normal circumstances the way the game runs is that the host
application loads the engine library, creates the main window and
creates the OpenGL window rendering context.

The engine library provides C type interface for creating the engine
object proper.

```
extern "C" DETONATOR_API engine::Engine* Detonator_CreateEngine();
```

Once the host app has loaded the engine library it can resolve the
C entry point, create the engine instance and move forward to the
main application loop:<br>

* Receiving user input from the application event queue
* Propagating the user input to the engine
* Calling update on the engine
* Callilng render on the engine
* Doing other activities outside the engine itself
  * For example timing frames and detecting jank!

#### Binary interop and DLL boundary

> [!NOTE]
>
> The following information only pertains to the native engine
> builds. When the engine is built for WASM there are no libraries
> but only a single WASM module.

Because of the complications regarding transient dependencies the
engine is built into a shared library with a binary interface that
hides the engine implementation details from the host application.
In other words the host application doesn't need to depend on any
of the internal details of the engine. However, there's a need to
share some infrastructure level components such as the logging
and threading systems.

The way this is solved is that the engine library also provides
an interface for accessing infrastructure components that are
built into the same library. This has no bearing on the logical
engine component it is only to facilitate sharing of infrastructure
level components without needing to add more shared libraries.

```
extern "C" DETONATOR_API void Detonator_CreateRuntime(interop::IRuntime**);
extern "C" DETONATOR_API void Detonator_CreateFileLoaders(Detonator_Loaders* out);
```

The se C entry points can then be used by the host application
to interact with the runtime infrastructure while maintaining a
strict binary firewall that helps use isolate the dependencies.


### Audio Subsystem

### Physics Subsystem

### Rendering Subsystem

### User Interface Subsystem

### Game Runtime