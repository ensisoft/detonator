DETONATOR 2D
=====================================

Build Instructions 👨🏼‍💻
--------------------------------------

![Screenshot](logo/cmake.png)
![Screenshot](logo/linux.png)
![Screenshot](logo/win10.png)
![Screenshot](logo/emscripten.png)

## Getting Started 👈

> [!TIP]
> The build process assumes some familiarity with building C++ based projects using
> toolchains and tools such as GCC, MSVS, Emscripten💩 and CMake.

> [!TIP]
> You'll need to have Qt5 in order to build the Editor.

> [!IMPORTANT]
> CMake 4x is not supported but you MUST have CMake 3.x
> A known working CMake version is 3.31.6

### Build Steps:
1. First build the engine and editor for your target platform.
2. If you want to deploy / package your game for web then build the engine for HTML5/WASM
3. If you want to work on the engine itself it's advisory to also run the (unit) tests.


## Step 1) Building the Editor & Engine for Desktop Windows 🪟

> [!IMPORTANT]
> These build instructions are for MSVS 2019 Community Edition 64bit build.

<details><summary>How to install dependencies</summary>

- Install Git version control system<br>
  https://git-scm.com/download/win


- Install Microsoft Visual Studio 2019 Community<br>
  https://www.visualstudio.com/downloads/


- Install prebuilt Qt 5.15.2<br>
  If the link doesn't work you'll need to create an account with the Qt company<br>
  and download the installer for the LGPL version (they like to hide this) from their site.<br>
  http://download.qt.io/official_releases/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.zip


- Install Conan package manager (VERSION 2)<br>
  https://docs.conan.io/en/latest/installation.html


- Install CMake build tool<br>
  https://cmake.org/install/

</details>


<details><summary>How to build the project in RELEASE</summary>

- Open `Developer Command Prompt for VS 2019`

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

<details><summary>How to build the project in DEBUG [OPTIONAL]</summary>

> Note that on MSVS the library interfaces change between debug/release build configs. (e.g. iterator debug levels).
> This means that in order to link to 3rd party libraries the debug versions of those libraries must be used.


- Open `Developer Command Prompt for VS 2019`

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

<details><summary>How to build Qt5 designer plugin [OPTIONAL]</summary>

```
  $ cd editor\gui\qt
  $ mkdir build
  $ cmake -G "Visual Studio 16 2019" -DCMAKE-BUILD_TYPE=Release
  $ cmake --build . --config Release
  $ cmake --install . --config Release
```

</details>



## Step 1) Building the Editor & Engine for Desktop Linux 🐧

<details><summary>How to install dependencies</summary>

*See your distro manuals for how to install the packages.*

Install these packages:

- GCC (or Clang) compiler suite
- CMake build tool
- Conan💩💩 package manager (VERSION 2)
  - On Archlinux you can use 'yay' to install conan + its dependencies from AUR*
  - See below for installing yay + conan💩💩
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

<details><summary>How to build the project in DEBUG [OPTIONAL]</summary>

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

<details><summary>How to build the project in PROFILE [OPTIONAL] </summary>

- Build the project for profiling using Valgrind / KCachegrind
```
  $ git clone https://github.com/ensisoft/detonator
  $ cd detonator
  $ git submodule update --init --recursive
  $ mkdir build_profile
  $ cd build_profile
  $ conan install .. --output-folder=conan --build missing -s build_type=RelWithDebInfo
  $ cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TOOLCHAIN_FILE=conan/conan_toolchain.cmake
  $ make -j16 install
```
- Then in order to profile and analyze the output use the combination of valgrind and kcachegrind.
  For example:
```
  $ cd detonator/audio/test/
  $ valgrind --tool=callgrind ./audio_test --graph
  $ kcachegrind callgrind.out.XXXXX
```
</details>

<details><summary>How to build with Ninja, Clang and Mold linker [OPTIONAL]</summary>

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

<details><summary>How to build Qt5 designer plugin [OPTIONAL]</summary>

```
  $ cd detonator/editor/gui/qt
  $ mkdir build
  $ cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Release
  $ make -j16
  $ sudo make install
```

</details>

<details><summary>Build troubleshooting 💩</summary>

When you create a Conan profile with

```
$ conan profile new default --detect
```

If Conan complains about "ERROR: invalid setting" (for example when GCC major version changes)
you can try edit ~/.conan/settings.yaml. Search for the GCC versions and edit there.

</details>

## Step 2) Building the Engine for HTML5/WASM 💩

> [!NOTE]
> HTML5/WASM build is only required if you want to build and package  your game for the web.<br>
> If you just want to try the editor or build native games you don't need this.

<br>
Some notes about building to HTML5/WASM.

* Building to HTML5/WASM is currently supported only for the engine but not the editor.
* Current Emscripten💩 version is 3.1.50. Using other version will likely break things.
* Building to HTML5/WASM will produce the required JS and WASM files to run games in the browser,<br>
  but you still need to build the editor in order to develop the game and package it.<br>
* When you package your game with the editor, the HTML5/WASM game files are copied by the editor<br>
  during the packaging process in order to produce the final deployable game package.

<details><summary>How to build on Linux 🐧</summary>

- Install Emscripten💩
```
  $ cd detonator
  $ git clone https://github.com/emscripten-core/emsdk.git
  $ cd emsdk
  $ git pull
  $ ./emsdk install 3.1.50
  $ ./emsdk activate 3.1.50
  $ source ./emsdk_env.sh
```
- Check your Emscripten💩 installation
```
  $ which emcc
  $ /home/user/emsdk/upstream/emscripten/emcc
  $ emcc --version
  $ emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.1.50 (047b82506d6b471873300a5e4d1e690420b582d0)
```
- Build the DETONATOR 2D engine into a WASM blob. Make sure that you have the Emscripten💩 tools in your path,
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

<details><summary>How to build on Windows 🪟</summary>

- Install Ninja🥷 build tool https://github.com/ninja-build/ninja/releases

  Drop the ninja.exe for example into the emsdk/ folder or anywhere on your PATH.


- Install Emscripten💩
```
  $ cd detonator
  $ git clone https://github.com/emscripten-core/emsdk.git
  $ cd emsdk
  $ git pull
  $ emsdk.bat install 3.1.50
  $ emsdk.bat activate 3.1.50
  $ emsdk_env.bat
```
- Check your Emscripten💩 and Ninja🥷 installation
```
  $ where emcc
  $ C:\coding\detonator\emsdk\upstream\emscripten\emcc
  $ C:\coding\detonator\emsdk\upstream\emscripten\emcc.bat
  $ emcc --version
  $ emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.1.50 (047b82506d6b471873300a5e4d1e690420b582d0)
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

<details><summary>Build troubleshooting 💩</summary>

Windows: Emscripten💩 3.0.0 build fails with

```
error: undefined symbol: _get_daylight (referenced by tzset_impl__deps: ['_get_daylight','_get_timezone','_get_tzname'], referenced by tzset__deps: ['tzset_impl'], referenced by localtime_r__deps: ['tzset'], referenced by __localtime_r__deps: ['localtime_r'], referenced by top-level compiled C/C++ code)
error: undefined symbol: _get_timezone (referenced by tzset_impl__deps: ['_get_daylight','_get_timezone','_get_tzname'], referenced by tzset__deps: ['tzset_impl'], referenced by localtime_r__deps: ['tzset'], referenced by __localtime_r__deps: ['localtime_r'], referenced by top-level compiled C/C++ code)
warning: __get_timezone may need to be added to EXPORTED_FUNCTIONS if it arrives from a system library
error: undefined symbol: _get_tzname (referenced by tzset_impl__deps: ['_get_daylight','_get_timezone','_get_tzname'], referenced by tzset__deps: ['tzset_impl'], referenced by localtime_r__deps: ['tzset'], referenced by __localtime_r__deps: ['localtime_r'], referenced by top-level compiled C/C++ code)
warning: __get_tzname may need to be added to EXPORTED_FUNCTIONS if it arrives from a system library
...
```

 * https://github.com/emscripten-core/emscripten/issues/15958
 * Current fix is to upgrade to Emscripten💩 3.1.10

Build fails with
```
wasm-ld: error: --shared-memory is disallowed by ldo.c.o because it was not compiled with 'atomics' or 'bulk-memory' features.
```

 * https://github.com/emscripten-core/emsdk/issues/790
 * This is trying to communicate that something was built without thread support when thread support should be enabled.<br>
   In other words trying to mix + match translation units / libs built with different build configuration.
 * Make sure to double check the build flags including `third_party/CMakeLists.txt`

</details>

If your build was successful there should now be `GameEngine.js`, `GameEngine.wasm` and `GameEngine.worker.js` files in the editor's `dist` folder.<br>

### Step 2.1) Deploying the Game for the Web 💩

When you package your game for the web the editor will copy all the required files to your chosen output directory.

* GameEngine.js
* GameEngine.wasm
* GameEngine.worker.js
* FILESYSTEM
* FILESYSTEM.js
* game.html

These 6 files are then all the files that you need to deploy/copy over to your web server.<br><br>

> [!TIP]<br>
> You can rename `game.html` to whatever you want, for example  `my-amazing-game.html`. Just don't change the names of any other files


<details><summary>1. Configure Your Web Server</summary>

<br>

> [!IMPORTANT]<br>
> You must enable the correct web policies💩 in order to enable SharedArrayBuffer💩 in order to enable threads !! 💩💩<br>
> Without SharedArrayBuffer web worker threads can't run and the engine cannot work. 💩💩<br>
> https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer


> [!IMPORTANT]<br>
> You must set the HTTP `Cross-Origin-Opener-Policy` to `same-origin`<br>
> You must set the HTTP `Cross-Origin-Embedder-Policy` to `require-corp`<br>
> You can achieve this with a `.htaccess` file.<br>

```
Header set Access-Control-Allow-Origin  "https://your-domain.com"
Header set Cross-Origin-Embedder-Policy "require-corp"
Header set Cross-Origin-Resource-Policy "same-site"
Header set Cross-Origin-Opener-Policy   "same-origin"
Header set Access-Control-Allow-Headers "range"
```

</details>

<details><summary>2. Deploy Your Game to Your Web Server</summary>

<br>

Copy the following files to your webserver using `sftp`or similar mechanism.<br>
You'll find these in your package output folder after the successful completion of your game packaging
in the editor.

```
  $ sftp my-user@my-server.com
  $ cd www\my-game\
  $ put GameEngine.js
  $ put GameEngine.wasm
  $ put GameEngine.worker.js
  $ put FILESYSTEM
  $ put FILESYSTEM.js
  $ put game.html
```

</details>


## Step 3) Running (Unit) Tests 🫣

<details><summary>Instructions to run unit tests</summary>

There's a bunch of unit tests that are built as part of the normal build process. Basically anything that begins with
a "*unit_test_*" is a unit test.<br>
For writing tests there's a very simple testing utility that is available in base. [base/test_minimal.h](base/test_minimal.h)

In addition to having the unit tests both the audio and graphics subsystems also have "rendering" tests, i.e. playing audio
or rendering stuff on the screen. The rendering tests rely on a set of *gold images* a.k.a. known good images.
Currently, the images are provided as part of this repository but there's the problem that because of differences
in OpenGL implementations it's possible that the rendering output is not exactly the same between various
vendors/implementations (such as NVIDIA, AMD, Intel etc. Fixing this is a todo for later). The audio tests, however,
don't have any automated way of verifying the test output.

* The expectation is that all unit test cases should pass on Linux
* The graphics rendering tests might have false negatives on different OpenGL vendors/driver versions/OS.
* Any graphics rendering test that differs from the expected gold image will generate *delta* and *result* images.
  * The *result* image will be the actual result image.
  * The *delta* image will help visualize the pixels that were not the same between the *result* and the *gold* image.
* Using multiple CMake "jobs" to run tests can confuse the rendering tests. 
  * A safer alternative is to *NOT* use the -jN parameter but only use a single job.
* If you want to run a single unit test application simply run the executable but please mind the current working folder.
  * On Windows some tests can fail because of this!
  * unit_test_workspace.exe needs to run in `build\Release` folder.
  * unit_test_image.exe needs to run in `build` folder.


#### [See this list for known Issues](ISSUES.md)

### On the Desktop Linux 🐧

<details><summary>How to run a single test</summary>

```
$ cd detonator/build
$ ./unit_test_workspace
$ ./unit_test_entity
$ ...
```

</details>

<details><summary>How to run all tests</summary>

Run all tests including unit tests, graphics and audio tests:

```
  $ cd detonator/build
  $ ctest -j16
```
</details>

<details><summary>How to run audio tests</summary>

Run mp3, ogg, flag and 24bit .wav tests. (Use --help for more information): 

```
  $ cd detonator/audio/test
  $ ./audio_test --mp3 --ogg --flac --24bit
  $ ...
  $ ./audio_test --help
```

</details>

<details><summary>How to run graphics tests</summary>

Run all tests with MSAA4. (Use --help for more information):

```
  $ cd detonator/graphics/test/dist
  $ ./graphics_test --test --msaa4
  $ ...
  $ ./graphics_test --help
```

</details>

### On the Desktop Windows 🪟

> [!IMPORTANT]<br>
> Before the tests can run you need to make sure that you have the Qt libraries available for launching the test applications.
> Either copy the libraries from the Qt bin folder to the build folder or add the Qt bin to your PATH

<details><summary>How to copy Qt libraries for testing</summary>

* For testing in release: 
  * Copy `QtCore.dll`, `QtGui.dll`, `QtNetwork.dll`, `QtSvg.dll`,  `QtWidgets.dll` and `QtXml.dll` to `detonator\build\Release` folder.
  * Copy Qt `plugins\platforms` folder to `detonator\build\Release` folder.
  * Copy Qt `plugins\imageformats` folder to `detonator\build\Release` folder.
<br><br>
* For testing in debug:
  * Copy `QtCored.dll`, `QtGuid.dll`, `QtNetworkd.dll`, `QtSvgd.dll`,  `QtWidgetsd.dll` and `QtXmld.dll` to `detonator\build_d\Debug` folder.
  * Copy Qt `plugins\platforms` folder to `detonator\build_d\Debug\` folder.
  * Copy Qt `plugins\imageformats` folder to `detonator\build_d\Debug\` folder.

</details>

<details><summary>How to run a single test</summary>

```
  $ cd detonator\build\Release
  $ unit_test_workspace.exe
  $ unit_test_entity.exe
  $ ...
```

</details>


<details><summary>How to run all tests</summary>

Release build

```
  $ cd detonator\build
  $ ctest -C Release
```

Debug build

```
  $ cd detonator\build_d
  $ ctest -C Debug
```

</details>


### On the Web (WASM+HTML5) 💩

> [!NOTE]<br>
> Currently, only some unit tests are available on the web. More tests will be enabled as needed.

<details><summary>How to run unit tests</summary>

After successful build the `detonator/emscripten/bin` folder should contain the following build artifacts:

  * http-server.py
  * unit-test.html
  * UnitTest.js
  * UnitTest.wasm
  * UnitTestThread.js
  * UnitTestThread.wasm
  * UnitTestThread.worker.js

1. Launch a web server for serving the test HTML pages.

```
  $ cd detonator/emscripten/bin
  $ python http-server.py
  $ Serving at port 8000
```

2. Open your web browser and navigate to http://localhost:8000/unit-test.html
3. Open your web browser and navigate to http://localhost:8000/unit-test-thread.html

The test execution may take some time. The performance tests will execute without running the JS main thread,
thus the page will seem "stuck" to the browser. But if you let it run it should complete and print `Success!`
to indicate completion.

</details>

</details>


### Random Notes on Building and Build Setup

<details><summary>How to install Conan💩💩 with Yay on ArchLinux</summary>

1. Download the yay package from AUR<br>
https://aur.archlinux.org/packages/yay <br>
<strong>BOTH YAY AND CONAN💩💩 WILL LIKELY HAVE MISSING DEPENDENCIES</strong>
   

2. Install missing Yay dependencies
```
$ sudo pacman -S debugedit
```

3. Build Yay
```
$ cd yay
$ makepkg
$ sudo pacman -U yay-12.3.5-1-x86_64.pkg.tar.zst
$ yay --version
$ yay v12.3.5 - libalpm v14.0.0
$
```

4. Use Yay to install Conan💩💩

```
$ yay -S conan
$ yay -S python-patch-ng
$ ...
$ conan --version
$ Conan version 2.6.0
$
```
</details>
