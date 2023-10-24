DETONATOR 2D 💥💣
===================

![Screenshot](screens/derp.gif "Starter demo")    
Menu with some simple easing curve based animations.

## User Interface Kit (UIK For Short) ##
UIK is a system to create  in-game user interfaces using a retained mode widget model. 
The widgets can be arranged in a hierarchy where container widgets contain other widgets. 
All widgets are then contained inside a window object which provides the "housing" of any
single game UI.

Current features:
* Retained mode keeps the static widget state persistent across UI saves and loads.  The UI editor
  in the DETONATOR 2D editor can be used to place widgets and change their properties effortlessly.
  There's no programming needed to create game UIs.
* Scriptable action handling through Lua scripting. Each UI/window class can be associated with a Lua
  script that can execute on various UI related events such as when a widget value changes etc. Its'  
  then possible to interface seamlessly with the rest of the game code from the UI script.
* Widget styling is based on JSON style files. Each style file provides material and property definitions
  with specific known keys to style widget and their sub-components. Then each widget can also be
  styled inside the UI editor by adjusting widget specific style properties.
  The styling has complete integration with the material system so that it's possible to use any material
  definition as widget material for things such as backgrounds, borders etc.
* Widget animation through a set of declarative animation rules. It's possible to create simple animations
  such as widget slides, bounces etc. with simple rule based animation system that defines a trigger event
  and any number of animation actions to be taken. The system is inspired by CSS keyframe animation and 
  offers several similar easing curves for quick and simple UI animation.
* Supports both keyboard and mouse input and allows complete control over any key mappings through virtual
  key system. In other words the UI system only reacts to "virtual keys" which can be mapped from physical
  keyboard key events through a key mapper.

Currently, the following widgets have been implemented.
* Form
* GroupBox
* Label
* ProgressBar
* Slider
* PushButton
* RadioButton
* CheckBox
* SpinBox (integer)
* ToggleBox

TODO widgets:
* Spinbox (float)
* Dropdown and/or ComboBox
* LineEdit (for text input)
* ListWidget

## Styling

Style file example:

```
   "properties": [
       {
          "key": "widget/edit-text-font",
          "value": "app://fonts/orbitron-medium.otf"
        }
        , ....
    ],
    "materials": [
        {
            "key": "slider/mouse-over/slider-knob",
            "type": "Gradient",
            "color0": "LightGray",
            "color1": "LightGray",
            "color2": "Black",
            "color3": "Black",
            "gamma": 2.2
        }, ....
    ]        
```

## Animation

Animation declaration example

```
$OnOpen
move 350.0 500
duration 1.0
interpolation EaseOutBounce

$OnClick
move -150.0 500.0
duration 1.0
interpolation EaseInBack
```

## Software Design

The UI system is completely abstract and doesn't do any rendering directly.
Instead, the widgets delegate their drawing operations to an abstract painter object
that can be implemented independently. One such implementation can be found under engine [UI](../engine/ui.cpp)

Both mouse and keyboard input are supported. The keyboard input is based on virtual keys
that can be generated in several possible ways. One typical way however is by using a 
virtual keyboard map, which maps "native" keyboard events (such as those provided by WDK)
to virtual keys recognized by the uikit. 


## Screenshots 

Below are some screenshots demonstrating the capabilities of the skinning system.

![HUD](screens/screenshot.png "Widgets with default styling.")  
Widgets with (mostly) default styling.

![Custom Style](screens/jungle.png "UI system screenshot with custom style")  
https://opengameart.org/content/jungle-cartoon-gui

![Custom Style](screens/kenney.png "UI system screenshot with custom style")

https://opengameart.org/content/ui-pack  
https://opengameart.org/content/ui-pack-space-extension
