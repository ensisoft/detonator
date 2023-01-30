DETONATOR 2D AUDIO 🎼
=====================

Overview 💭
---------------------
The audio subsystem is divided into following core components that take part in playing audio.

* Device 
   - Abstract interface for accessing some underlying platform API for playing audio.
   - Uses PulseAudio on Linux, Waveout on Win32 and OpenAL on Emscripten
* Player
  - Takes care of managing audio playback by using some audio playback device.
  - Supports running a native audio thread for audio playback
* Source
  - A low level source object for filling audio buffers with PCM data
  - Called by the playback stream object that exists on some device whenever the playback device asks for more PCM data
* Stream
  - Device specific audio playback stream with some state. Sources its data from its Source object.
  - Exists only inside the device and is not accessible from outside the device during playback.
    
Overview of the basic steps needed to play audio:
1. Create an audio device through *audio::Device::Create* 
2. Use the device to create an *audio::Player*
3. Create an audio *Source* object
4. Call *Player::Play* and pass the source as a parameter. The return value will be a "handle" to the stream in the player.
   1. The audio player will take ownership of the source
   2. The player will use the device to create a playback *Stream* object
   3. The player will store the stream in an internal "currently playing" track list
   4. The player will observe the stream for state changes, manage its playback and fill PCM buffers 
      1. *Source::HasMore* is used to check whether the *Source* has more PCM data or not.
      2. *Source::FillBuffer* is used to provide PCM data to underlying audio device
5. Periodically call *audio::Player::GetEvent* to receive *SourceCompleteEVent* information regarding the 
   completion of some audio playback. 
6. (Optional) call *Player::Pause*, *Player::Resume* or *Player::Cancel* to manage the playback of the audio. 

Currently supported PCM formats:
* Mono or stereo
* 16bit integer (All backends) 
* 32bit integer (Pulseaudio/Waveout)
* 32bit floating point (All backends)
* Various sample rates from 8k to 96k (depending on the underlying audio device support)

Audio Graphs 🤔
-------------
The goal is to be able to create an audio system that can be used to not only playback simple audio files but to also
add/create various audio effects (such as echo, delay etc.) in realtime during the said playback.

The graph consists of the following:
* Audio elements
  - Have any number of input/output ports for receiving (input) and providing (output) PCM audio buffers
    - Each port has an associated PCM format that it supports.   
  - Source elements have 0 input buffers and either generate (read a file) PCM data for the rest of elements.
  - Other elements receive PCM data through their input port(s), perform signal processing on it and push their
    output data into their output ports.
- Audio links
  - The connections between elements' ports'.
  - A single audio link connects element A's output port to element B's input port. 

When the graph evaluates the elements are evaluated in a topological order. Any output buffer from a preceding 
element is/are *pulled* from its output port(s) and *pushed* into the receiving/subsequent element's input port(s). 
Then the subsequent element is evaluated. This process repeats as long as there are more elements in the topological 
list of elements to evaluate. The graph is assumed to form a DAG  (https://en.wikipedia.org/wiki/Directed_acyclic_graph)
and may not contain any cycles. Note that the so-called *source* (such as *FileSource*) elements do not have any 
input ports and cannot be used to receive any data. They can only be used to generate/provide data for the rest of the
elements in the graph.

The *audio::AudioGraph* class implements the *audio::Source* interface and can be sent to the *audio::Player*
for playback.

See [elements](element.h) for a list of audio elements.

Third Party Libs 📚
-------------------
Audio system depends on the following 3rd party libraries:
* libsndfile
  - Used to decode .flac, .ogg (vorbis) and .wav files
  - https://libsndfile.github.io/libsndfile/
* mpg123
  - Used to decode .mp3 files
  - http://www.mpg123.de
* libsamplerate
  - Used to do sample rate conversions on the fly when needed
  