DETONATOR 2D
=====================================

Build Instructions 👨🏼‍💻
--------------------------------------

![Screenshot](logo/cmake.png)
![Screenshot](logo/linux.png)
![Screenshot](logo/win10.png)
![Screenshot](logo/emscripten.png)

HTML5/WASM (Emscripten)
------------------------------

Some notes about building to HTML5/WASM.

* Building to HTML5/WASM is currently supported only for the engine but not the editor.
* Current Emscripten version is 3.0.0. Using other version will likely break things.
* Building to HTML5/WASM will produce the required JS and WASM files to run games in the browser,<br> 
  but you still need to build the editor in order to develop the game and package it.<br>
  When you package your game through the editor the HTML5/WASM game files are copied by the editor<br>
  during the packaging process in order to produce the final deployable output.

### Thread Support
Supporting threads using pthread requires some more complicated steps in the Emscripten build AND in the game deployment.

1. Turn on the pthread support in the Emscripten build in [emscripten/CMakeLists.txt](emscripten/CMakeLists.txt)

```
   target_compile_options(GameEngine PRIVATE -pthread)
   ...
   target_link_options(GameEngine PRIVATE -pthread)
   ...
   install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.worker.js" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist")
```

2. When you deploy your game to a web server the host must turn on HTTP Cross-Origin policy flags in order to enable SharedArrayBuffer.<br>
   https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer

.htaccess file
```
Header set Access-Control-Allow-Origin  "https://your-domain.com"
Header set Cross-Origin-Embedder-Policy "require-corp"
Header set Cross-Origin-Resource-Policy "same-site"
Header set Cross-Origin-Opener-Policy   "same-origin"
Header set Access-Control-Allow-Headers "range"
```

#### Audio Thread

The audio engine inside DETONATOR 2D engine can use a separate audio thread for audio playback.<br>
There are several build variables that control this feature.

[escripten/config.h](emscripten/config.h)

```
  #define AUDIO_USE_LOCK_FREE_QUEUE
  #define AUDIO_USE_PLAYER_THREAD
```

### Building for WASM

<details><summary>How to build on Linux</summary>

- Install Emscripten
```
  $ cd detonator
  $ git clone https://github.com/emscripten-core/emsdk.git
  $ cd emsdk
  $ git pull
  $ ./emsdk install latest
  $ ./emsdk activate 3.0.0
  $ source ./emsdk_env.sh
```
- Check your Emscripten installation
```
  $ which emcc
  $ /home/user/emsdk/upstream/emscripten/emcc
  $ emcc --version
  $ emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.0.0 (3fd52e107187b8a169bb04a02b9f982c8a075205)
```
- Build the DETONATOR 2D engine into a WASM blob. Make sure that you have the emscripten tools in your path,
  i.e. you have sourced emsdk_env.sh in your current shell.
```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ cd emscripten
  $ mkdir build
  $ cd build
  $ emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
  $ make -j16 install
``` 
</details>

<details><summary>How to build on Windows</summary>

- Install Ninja built tool https://github.com/ninja-build/ninja/releases
 
  Drop the ninja.exe for example into the emsdk/ folder or anywhere on your PATH.


- Install Emscripten
```
  $ cd detonator
  $ git clone https://github.com/emscripten-core/emsdk.git
  $ cd emsdk
  $ git pull
  $ emsdk.bat install latest
  $ emsdk.bat activate 3.0.0
  $ emsdk_env.bat
```
- Check your Emscripten and Ninja installation
```
  $ where emcc
  $ C:\coding\detonator\emsdk\upstream\emscripten\emcc
  $ C:\coding\detonator\emsdk\upstream\emscripten\emcc.bat
  $ emcc --version
  $ emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.0.0 (3fd52e107187b8a169bb04a02b9f982c8a075205)
  $ where ninja
  $ C:\coding\detonator\emsdk\ninja.exe
  $ ninja --version
  $ 1.10.2
```
- Build the DETONATOR 2D engine into a WASM blob. Make sure you have emcc and Ninja in your path i.e. you have
  ran emsdk_env.bat in your current shell.
```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ cd emscripten
  $ mkdir build
  $ cd build
  $ emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
  $ ninja -j16
  $ ninja -j16 install
```
</details>

If everything went well there should now be 'GameEngine.js' and 'GameEngine.wasm' files in the editor's dist folder.
The .js file contains the JavaScript glue code needed to manhandle the WASM code into the browser's WASM engine
when the web page loads. When a game is packaged for web these files will then be deployed (copied) into the
game's output directory.

When threading is turned on in the Emscripten build an additional 'GameEngine.worker.js" file is produced.
This file contains some required JavaScript glue code for the web workers that implement the pthread support in Emscripten.
This file must then be deployed alongside the GameEngine.js, GameEngine.wasm etc files.


Desktop Linux
-------------------------------------

<details><summary>How to install dependencies</summary>

*See your distro manuals for how to install the packages.*

Install these packages:

- GCC (or Clang) compiler suite
- CMake build tool
- Boost C++ libraries
- Conan package manager (VERSION 2)
    - *On Archlinux you can use 'yay' to install conan + its dependencies from AUR*
- Git version control system
- Qt5 application framework

</details>

<details><summary>How to build the project in RELEASE</summary>

```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build
  $ cd build
  $ conan install .. --output-folder=conan --build missing
  $ cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan/conan_toolchain.cmake
  $ make -j16 install
  $ ctest -j16
```
</details>

<details><summary>How to build the project in DEBUG</summary>

```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build_d
  $ cd build_d
  $ conan install .. --output-folder=conan --build missing -s build_type=Debug
  $ cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=conan/conan_toolchain.cmake
  $ make -j16 install
  $ ctest -j16
```
</details>

<details><summary>How to build the project in PROFILE</summary>

- Build the project for profiling using valgrind / kcachegrind
```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build_profile
  $ cd build_profile
  $ conan install .. --build missing 
  $ cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
  $ make -j16 install
```
- Then in order to profile and analyze the output use the combination of valgrind and kcachegrind.
  For example:
```
  $ cd detonator/audio/test/
  $ valgrind --tool=cachegrind ./audio_test --graph
  $ kcaghegrind cachegrind.out.XXXXX
```
</details>

<details><summary>How to build with Ninja, Clang and Mold linker (OPTIONAL)</summary>

- These are alternative instructions for build using Ninja, Clang and Mold linker.

```
  $ export CC=/usr/bin/clang
  $ export CXX=/usr/bin/clang++
  $ conan profile new detonator-clang --detect
  
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build
  $ cd build
  $ conan install .. --build missing --profile detonator-clang
  $ cmake -G "Ninja" .. -DCMAKE_BUILD_TYPE=Release -DUSE_MOLD_LINKER=ON 
  $ ninja -j16 install
```
</details>

<details><summary>How to build Qt5 designer plugin (OPTIONAL)</summary>

```
  $ cd detonator/editor/gui/qt
  $ mkdir build
  $ cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Release
  $ make -j16 
  $ sudo make install
```

</details>

<details><summary>Build troubleshooting</summary>

When you create a Conan profile with

```
$ conan profile new default --detect
```

If Conan bitches about "ERROR: invalid setting" (for example when GCC major version changes)
you can try edit ~/.conan/settings.yaml. Search for the GCC versions and edit there.

</details>

Desktop Windows
---------------------------------

These build instructions are for MSVS 2019 Community Edition and for 64bit build.

<details><summary>How to install dependencies</summary>

- Install Git version control system
  https://git-scm.com/download/win

- Install Microsoft Visual Studio 2019 Community
  https://www.visualstudio.com/downloads/

- Install prebuilt Qt 5.15.2
  http://download.qt.io/official_releases/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.zip

- Install Conan package manager (VERSION 2)
  https://docs.conan.io/en/latest/installation.html

- Install CMake build tool
  https://cmake.org/install/



</details>


<details><summary>How to build the project in RELEASE</summary>

- Open "Developer Command Prompt for VS 2019"

```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build
  $ cd build
  $ conan install .. --output-folder=conan --build missing
  $ cmake -G "Visual Studio 16 2019" .. -DCMAKE_BUILD_TYPE=Release  -DCMAKE_TOOLCHAIN_FILE=conan/conan_toolchain.cmake
  $ cmake --build   . --config Release
  $ cmake --install . --config Release
```

</details>

<details><summary>How to build the project in DEBUG</summary>

_Note that on MSVS the library interfaces change between debug/release build configs. (e.g. iterator debug levels).
This means that in order to link to 3rd party libraries the debug versions of those libraries must be used._

- Open "Developer Command Prompt for VS 2019"

```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build_d
  $ cd build_d
  $ conan install .. --output-folder=conan --build missing -s build_type=Debug
  $ cmake -G "Visual Studio 16 2019" .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=conan/conan_toolchain.cmake
  $ cmake --build   . --config Debug
  $ cmake --install . --config Debug
```

</details>

<details><summary>How to build Qt5 designer plugin (OPTIONAL)</summary>

```
  $ cd editor\gui\qt
  $ mkdir build
  $ cmake -G "Visual Studio 16 2019" -DCMAKE-BUILD_TYPE=Release
  $ cmake --build . --config Release
  $ cmake --install . --config Release
```

</details>


Running (Unit) Tests 🫣
-----------------------------

There's a bunch of unit tests that are built as part of the normal build process. Basically anything that begins with
a "*unit_test_*" is a unit test.  
For writing tests there's a very simple testing utility that is available in base. [base/test_minimal.h](base/test_minimal.h)

In addition to having the unit tests both the audio and graphics subsystems also have "rendering" tests, i.e. playing audio
or rendering stuff on the screen. The rendering tests rely on a set of *gold images* a.k.a. known good images.
Currently, the images are provided as part of this repository but there's the problem that because of differences
in OpenGL implementations it's possible that the rendering output is not exactly the same between various
vendors/implementations (such as NVIDIA, AMD, Intel etc. Fixing this is a todo for later). The audio tests, however,
don't have any automated way of verifying the test output.

#### [See this list for known Issues](ISSUES.md)

### On the desktop (Linux/Windows)

<details><summary>How to run all tests</summary>

*Currently, the expectation is that all cases should pass on Linux. On Windows some tests are unfortunately broken.*
* In order to run tests after a successful build:

```
  $ cd detonator/build
  $ ctest -j16
```
</details>

<details><summary>How to run audio tests</summary>

* Runs, mp3, ogg, flag and 24bit .wav tests. Use --help for more information.
```
  $ cd detonator/audio/test
  $ ./audio_test --mp3 --ogg --flac --24bit
  $ ...
  $ ./audio_test --help
```

</details>

<details><summary>How to run graphics tests</summary>

*Any test rendering that differs from the expected gold image will stop the program for user input
(press any key to continue) and will generate a *Delta_* and *Result_* images. The former will help visualize the pixels
that were not the same between result and gold and the result will be actual rendering result.*

* Run all tests with MSAA4. Use --help for more information

```
  $ cd detonator/graphics/test/dist
  $ ./graphics_test --test --msaa4
  $ ...
  $ ./graphics_test --help 
```

</details>

### On the Web (WASM+HTML5)
*Currently, only some unit tests are available on the web. More tests will be enabled as needed.*

<details><summary>How to run unit tests</summary>

The detonator/emscripten/bin folder should contain the following build artifacts:
* unit-test.html
* UnitTest.js
* UnitTest.wasm

Launch a web server for serving the test HTML page.

```
  $ cd detonator/emscripten/bin
  $ python -m http.server
```

Open your web browser and navigate to http://localhost:8000/unit-test.html.

</details>