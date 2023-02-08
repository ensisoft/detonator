# DETONATOR 2D

## Known Issues 🤬

List of known issues that are not clearly identified as DETONATOR 2D bugs (yet).

### Linux

Pulseaudio  audio issue that happens occasionally. Pulseaudio sound is crackling a lot and several buffer 
underruns are reported during the stream playback. Increasing audio buffer size or the audio device poll frequency makes no difference.<br>
Audio test cases might hang and never finish.

Issue started happening after some distro level system update.<br> 
Pulseaudio version 16.1

Current workaround is to try to restart pulseaudio.
```
$ pulseaudio -k
$ pulseaudio --start
```

### Windows

### HTML5/WASM