# DETONATOR 2D 💣💥

## Known Issues 🤬

List of known issues that are not clearly identified as DETONATOR 2D bugs (yet).

### Intel Optimus

<details><summary>GL_EXT_sRGB support is a LIE</summary>

* Intel's drivers (at least on Optimus) report `GL_EXT_sRGB` support but when you actually try to 
create a texture and use `GL_SRGB_EXT` or `GL_SRGB_ALPHA_EXT` formats `GL_INVALID_OPERATION` is reported.

* This issues doesn't really matter since ES3 has native `sRGB` texture support.
* Tested on Windows with driver version xxx

</details>

### NVIDIA 

### HTML5/WASM 

<details><summary>HTML5/WASM threading issues</summary>

Supporting threads using pthread requires some more complicated steps in the Emscripten build AND in the game deployment.

1. Turn on the pthread support in the Emscripten build in [emscripten/CMakeLists.txt](emscripten/CMakeLists.txt)

```
   target_compile_options(GameEngine PRIVATE -pthread)
   ...
   target_link_options(GameEngine PRIVATE -pthread)
   ...
   install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GameEngine.worker.js" DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../editor/dist")
```

2. When you deploy your game to a web server the host must turn on HTTP `Cross-Origin` policy flags in order to enable `SharedArrayBuffer`.<br>
   https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer 
   <br>This can be done with a `.htaccess` file. See sample below 

```
Header set Access-Control-Allow-Origin  "https://your-domain.com"
Header set Cross-Origin-Embedder-Policy "require-corp"
Header set Cross-Origin-Resource-Policy "same-site"
Header set Cross-Origin-Opener-Policy   "same-origin"
Header set Access-Control-Allow-Headers "range"
```

3. The threads must be launched by the main thread from the browser's main event loop.
   In other words if your code does:

```
    auto thread = std.:thread(...);
    thread.join(); 
```

You're deadlocking forever since the main thread (the current thread) must return to the
browser's event loop in order to launch the thread, but joining the thread will block it so it cannot
return to the event loop and the thread never launches and finishes. Turds all the way down!

The way to work around this is to use the Emscripten `-sPTHREAD_POOL_SIZE=n` build parameter which adds
code to initialize N number of threads on the application launch so that they're ready to go.

</details>

<details><summary>Messed up Content-Type header</summary>

Issues with HTTP 'Content-Type' header being set incorrectly.
* Python's http module fails to identify and set the Content-Type properly on .js GET
    * Manually adding 'Content-Type' header in the 'end_headers' method adds a *second*
      'Content-Type' header.
* Chrome gets confused about what is the actual ContentType and conflicting errors appear.
* Using threads (inclusion of .worker.js ?) causes the Content-Type issue to appear
* Failures to load the JS code in the browser. Errors (from Chromium) include:

```
Refused to execute script from 'http://localhost:8000/GameEngine.js' because its MIME type ('text/plain') is not executable.
self.onmessage @ GameEngine.worker.js:1

worker.js onmessage() captured an uncaught exception: NetworkError: Failed to execute 'importScripts' on 'WorkerGlobalScope': The script at 'http://localhost:8000/GameEngine.js' failed to load.
threadPrintErr @ GameEngine.worker.js:1
self.onmessage @ GameEngine.worker.js:1

GameEngine.worker.js:1 Error: Failed to execute 'importScripts' on 'WorkerGlobalScope': The script at 'http://localhost:8000/GameEngine.js' failed to load.
    at self.onmessage (GameEngine.worker.js:1:1386)

Refused to execute script from 'http://localhost:8000/GameEngine.js' because its MIME type ('text/plain') is not executable.
self.onmessage @ GameEngine.worker.js:1
```

</details>


### LINUX (Native)

<details><summary>Pulseaudio  audio issue that happens occasionally.</summary>

Pulseaudio sound is crackling a lot and several buffer 
underruns are reported during the stream playback. Increasing audio buffer size or the audio device poll frequency makes no difference.<br>
Audio test cases might hang and never finish.

Issue started happening after some distro level system update.<br> 
Pulseaudio version 16.1

Current workaround is to try to restart pulseaudio.
```
$ pulseaudio -k
$ pulseaudio --start
```

</details>

<details><summary>glib fails randomly at runtime</summary>

Random glib failure (seen when running unit tests such as unit_test_gui and unit_test_ipc)
```
[enska@arch build]$ ./unit_test_gui

(process:17817): GLib-CRITICAL **: 13:03:37.700: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_lookup: assertion 'hash_table != NULL' failed

(process:17817): GLib-CRITICAL **: 13:03:37.701: g_hash_table_insert_internal: assertion 'hash_table != NULL' failed
**
GLib-GObject:ERROR:../glib/gobject/gtype.c:2876:g_type_register_static: assertion failed: (static_quark_type_flags)
Bail out! GLib-GObject:ERROR:../glib/gobject/gtype.c:2876:g_type_register_static: assertion failed: (static_quark_type_flags)
Aborted (core dumped)
```

This smells like GIANT FUCKING DOOKIE regarding some global state and associated initialization order that is undefined
and randomly starts failing. 

The "fix" is to try to shuffle some target_link_libraries in the CMakeList.txt around to see if the issue resolves.
The real fix would be to remove amateurs from ever contributing any fucking code anywhere in the world. If you can't
do better than this just do everyone a fucking favour and don't ever write any publicly released code. Thank you!

</details>

<details><summary>Qt5 QWindow containers are broken on Wayland</summary>

Qt5 QWindow based rendering with `createWindowContainer` is broken under Wayland and causes rendering glitches 
and input problems:

- The window rendering is glitchy and sometimes doesn't render when the tabs are in the main window or there
  will be rendering artifacts.
- Input events are not provided correctly (key presses etc)

Trying to set the `QT_QPA_PLATFORM=xcb` fixes these problems but causes severe breakage in the graphics
stack that manifest themselves with random crashes inside the OpenGL driver in some arbitrary API call
such as glBufferData.

Currently it looks like Qt5 + Wayland is a NO-GO. Let's check back in another 10 years or so.


</details>


<details><summary>MSAA16 config is not available on Wayland</summary>

* Trying to run the graphics test app with MSAA16 setting causes an exception since no match config is available.
* Tests are failing due to this.

</details>

### WINDOWS (Native)

<details><summary>MSVS2022 build no longer Debuggable with Qt 5.15.2</summary>

* Trying to launch the editor's debug build produces a runtime error about some "#%"¤#@@@QString::@£@£@£@FuckYou not 
being found. 
* Root cause is probably some change in the MSVS compiler infrastructure and binary compatibility across various versions 
  since the prebuilt Qt package uses msvs2019 binaries.

</details>

### Qt5 Quirks and bugggggsss 🤬

<b>!!!!  NEVER TRUST THE DESIGNER BUT ALWAYS CHECK THE XML!!!1 !!!</b>

<details><summary>QPushButton fucks up with `auto default`</summary>

* Buttons when added to a dialog can have 'auto default' property which creates surprising
  behaviour when 'enter' is pressed. The button will capture the 'enter' and accept the dialog.
* Designer often borks and fails to show the auto default properly. DON'T TRUST THE DESIGNER!
* Sometimes bug happens when there's *no* autoDefault property in the .xml. Make sure to *ADD* autoDefault with false.
* Check the generated .xml for the auto default for the real value.

```
     <widget class="QPushButton" name="btnCancel">
       <property name="text">
        <string>Cancel</string>
       </property>
       <property name="autoDefault">
        <bool>false</bool>
       </property>
      </widget>
```

</details>


<details><summary>QComboBox creates layout issues with size adjust policy </summary>

* This is about the way the QComboBox absolutely wants to resize itself according to it's content.
  Intention is probably all good but in practice this will often cause weird layout issues and mis-alignment.
* There's no "don't adjust contents" policy, but looks like using "fixed" size policy helps?

</details>

<details><summary>QDialog exec() no longer modal after show()</summary>

```
void MainWindow::something()
{
    DlgFoobar dlg(this);
    dlg.show();
    ...
    dlg.exec();
}
```

Calling show before exec makes the dialog lose modality.
In other words the user can click on other controls in the mainwindow.

Comments on exec()

So calling exec() without show() being called first works as expected. The dialog remains modal and the user is restricted to only applying input on the dialog.

The problem is that using exec() is not adequate.

Consider a case where you wish to restore the dialog state to a previous state, i.e. populate the dialog and its data in some meaningful way.

To do this with smooth UX you really need to:

1. Load and restore the dialog geometry *before* showing the dialog.<br>
   If you first show and then restore the resizing will cause ugly visual flicker and disturbance.
2. Show it after the geometry has been restored
3. Then load any other state, including state that might fail (for example imagine a file is no longer available)<br>
   3.1 You need to have the dialog open so that you can open a QMessageBox properly using the dialog as the window parent in order to open the msg box properly.<br>
   3.2 Finally call exec() and block the user to the dialog.

</details>   

<details><summary>QScrollAeaWidget simply HATES YOU</summary>

- https://www.youtube.com/watch?v=8xvZ3Qj1KVk

</details>


### Other Issues

<details><summary>Short audio clips don't  produce audio</summary>

* Trying to play back short audio clips (~max 0.5s or so) don't produce any actual audible audio.
* Issues is re-producible with other software too such as browsers.
* Happens both on Linux and Windows
* This seems to be an audio topology / routing issue. In my setup I audio routed to my display via Thunderbolt 
and from the display there's an analog connection to the soundbar. Playing the audio directly
on the laptop resolves the issue.

</details>
