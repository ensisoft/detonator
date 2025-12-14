# DETONATOR 2D 💥💣

## Graphics Library

This covers the graphics library in graphics/
Main classes are materials and drawables.

### Materials
todo:

### Drawables

Drawables are objects that contain state and that use their state to produce
geometry data on the GPU. Essentially they implement some algorithm to create
geometry data on the GPU by loading it from the file or by creating it programmatically.


#### Drawable Classes

Drawable class objects are **types** that the user defines. Each object of a
drawable class represents a **type** at runtime and all drawable instances of
the type share the same fundamental characteristics and behavior. For example
the user can create a new partice engine called "sparkle". This class has
some parameters that control how the particles move, get spawned and killed.

Each instance of the the "sparkle" particle engine then behaves the same
way while having their own instance lifetime state. For example if one
instance has just begun its life it might just be in the initial state
of spawning particles while another instance that is nearing the end of
its lifetime is already fading out its particles.

#### Drawable Instances

Drawable instance objects represents **instances** of some drawable type.
Their behaviour and properties are defined by the class object but they
all have their own instance state as applicable to the type. Most of the
instance state is limited to time to allow time based animation of the
drawable geometry as the time passes.

##### ParticleEngineInstance
 - Animates particle motion over time. Produces streaming geometries
   on the GPU that need to be updated on every single frame.
 - New types can be created by the user in the particle editor.

##### SimpleShapeInstance
  - Primitive "simple" shapes that are produced via some hard coded
    data tables or programmatically/algorithmically. These are your
    fundamental shapes, cubes, circles, rectangles etc.

##### PolygonMeshInstance
  * More complex polygonal data that can contain multiple draw commands
    and submeshes where each submesh is mapped to a different material key.
  * Loaded from a data file (or in some cases from in-line data stored
    in the class object).
  * New types can be created by the user in the polygon/shape editor.
  * New types can be created by the user by importing a model file such
    as FBX, glTF or OBJ.


#### Drawable Properties

Drawables have properties that let the rest of system to classify
and deal with them. Internally each drawable implementation knows
the type of data (vertex and index format) it produces and what
kind of vertex shader is needed. When a drawable is drawn together
with material the painter will combine the vertex shader and material
shader together to produce the final shader program as needed.

##### Draw Primitive
- Defines whether the drawable is producing data that is rasterized
  as *points*, *lines*, or *triangles*. Triangles includes any kind of
  triangle topology, i.e. GL_TRIANGLES, GL_TRIANGLE_FAN or GL_TRIANGLE_STRIP

##### Draw Category
 - When something is being drawn we need to know what's the logical
  reprensentation of the thing we're drawing. This is because some
  material shaders might want to perform material animation based
  on whether we're using the shader to draw *particles* or *tiles*
  or *basic shapes*. This also lets us write some uber shaders
  where a single material shader can support many types of drawables
  but some behavior in the shader must change.

##### Spatial Mode
 * Defines the drawable spatial extents and dimensionality.
   * True3D
     * The drawable represents proper 3D shape with dimensionality
       across three axes (x, Y and Z). Cube, Sphere etc.
   * Flat2D
     * The drawable a pure 2D shape with dimensionality across
       two axes (X and Y). Rectangle, circle ec.
   * Perceptual3D
     * The drawable is a hybrid 2D/3D shape where the 2D part
       represents a user facing billboard, (typically) or an
       axonometric tile. The associated material has a prerasterized
       3D shape from a certain angle.
     * The shape *can* carry limited/partial 3D information
       in order to support some 3D features such as lights.

### Other Classes

todo:

## Rendering Technicalities

#### Gamma (in)Correction (and sRGB)

<details><summary>Gamma Correction Overview</summary><br>
Gamma (in) correction is the process of applying broken information in a futile attempt to
try to understand how to do gamma/sRGB correct rendering. The magic is archaic and not well understood and
full of caveats. And the first thing you need to know about gamma correction is that any single
document you read is likely incorrect.

If it ever "works" it's only because of luck and bugs cancelling each other out.
Hence the term *incorrection* which is 10x more accurate than gamma *correction*. ;-)

Gamma (in)correction is essentially done because standard 8bit per channel image formats don't
offer enough precision to have good dark shades and this is important because the human vision
system is more sensitive to differences in darker colors (in real life this is sensitivity to brightness)
than to lighter colors. In other words we can see more shades of "darkness" than shades of "brightness".

So therefore if we allocate the 8bits and (256 possible values) per channel linearly so that both bright
color values (i.e. above the 129 mid point) have the same weight as the darker color values (below the 128 mid point)
the result is not having enough shades of dark colors and too many (wasted) shades of bright colors,
that the human eyeballs cannot distinguish leading to loss of perceptible precision.

The "fix" is to to apply gamma *encoding* function so that small darker shades are allocated and stored with
more bits than the brighter color values are.

If we imagine working with high precision color information (think floats in image processing)  the gamma
function is a non-linear mapping from the incoming color value to a different value which is then used to
convert the value into 8bit integer to get the value that is stored on disk.

For example if take input value 0.21 and apply gamma encoding with g = 2.2 we get

x  = 0.21^(1.0/2.2)<br>
x ~= 0.49

If we convert this to unsigned 8bit and we get 125 instead of 53.

*sRGB is an official/standardized color space for storing images in RGB color model but with
an encoding very similar to the gamma encoding explained above. (The curve is not exactly the same but the principle
is the same). So people might talk about "gamma encoding"  when in reality they really mean sRGB*

This of course means that when the data is displayed the inverse process (gamma encoding) must take place.
So imagine you want to view the image that was stored in gamma encoded 8bit format.
Clearly the data is *encoded* for better precision but how can it be displayed?

</details>

<details><summary>Gamma Correction History</summary><br>
Back in the day the CRT displays had a non-linear mapping from input values to output brightness so that the
actual human eyeball perceived value would be less than what the input would presume. In other words there would
be some logarithmic decay of input signal to output signal. For example if your program would have told the monitor
to display a value 0.5 the actual brightness your eyeballs would have perceived would have been around 0.2!

It just happens that this decay (which is also called gamma) is *very* similar to the inverse of the gamma
*encoding* curve that was described above. Your actual output would approximate

pixel p = 0.5 = 0x80 = 127<br>
gamma g = 2.2<br>

o  = p^g<br>
o  = 0.5^2.2<br>
o ~= 0.2

So theoretically if you had access to the direct frame buffer that is used to drive the signal to your monitor
your display program could simply open that image file, copy those bytes into the frame buffer and voila
the gamma decoding would take place automatically in the CRT circuitry.

Your 0x80 (127) byte value would drive 0.2 of the display pixels maximum brightness for that color channel.
(In practice of course every pixel and every monitor would vary slightly and produce slightly different
result but the theory applies).

Yet because this process is so ingrained in the graphics stacks even today modern LED and LCD displays apply the same gamma decoding
curve to their input in order to keep things backwards compatible.

</details>


<details><summary>Gamma Correction in OpenGL ES2</summary><br>

**DEFAULT FRAME BUFFER**

ES2 spec offers no explicit method for the OpenGL ES2 API to control the sRGB setting. Instead the spec
defers this decision to EGL. Of course EGL is not the only way to create OpenGL ES2 rendering contexts
since also WGL and GLX (and AGL) can be used with the right extensions.

<i>
"The effects of GL commands on the window-system-provided framebuffer are
ultimately controlled by the window-system that allocates framebuffer resources.
It is the window-system that determines which portions of this framebuffer the GL
may access at any given time and that communicates to the GL how those portions
are structured. Therefore, there are no GL commands to configure the window-
system-provided framebuffer or initialize the GL. Similarly, display of framebuffer
contents on a monitor or LCD panel (including the transformation of individual
framebuffer values by such techniques as gamma correction) is not addressed by
the GL. Framebuffer configuration occurs outside of the GL in conjunction with the
window-system; the initialization of a GL context occurs when the window system
allocates a window for GL rendering. The EGL API defines a portable mechanism
for creating GL contexts and windows for rendering into, which may be used in
conjunction with different native platform window systems."
</i>

The EGL_KHR_gl_colorspace extension for asking for sRGB enabled window system
render targets then goes on to say this:

<i>
"Only OpenGL and OpenGL ES contexts which support sRGB rendering must respect
requests for EGL_GL_COLORSPACE_SRGB_KHR, and only to sRGB formats supported
by the context (normally just SRGB8). Older versions not supporting sRGB rendering
will ignore this surface attribute."
</i>

So basically it seems that if the ES2 implementation has decided to support
sRGB rendering (which it might have decided not to do) will only support that
with the EGL created rendering surface when the surface is created with the
EGL_KHR_gl_colorspace's sRGB flag.

NOTE: On the desktop GL a GL_FRAMEBUFFER_SRGB flag is used to control the frame buffer
writing and the encoding of the fragment color values.

https://www.khronos.org/opengl/wiki/Framebuffers

=> When GL_FRAMEBUFFER_SRGB is enabled *and* the RT color target is in
   sRGB colorspace then the fragment written will be converted into sRGB
   by the implementation. This will also work with blending so that the
   dst already in the color buffer will be converted properly before
   mixed with the incoming color value.
   **GL_FRAMEBUFFER_SRGB doesn't exist in GL ES2.**

</details>

<details><summary>Gamma Correction in OpenGL ES3</summary><br>
todo:
</details>


<details><summary>Gamma Correction in OpenGL WebGL 1</summary><br>
LOL ! :D :D LD
</details>


<details><summary>Gamma Correction in OpenGL WebGL 2</summary><br>
todo
</details>

<details><summary>Gamma Correction in OpenGL in Practice</summary><br>
In real life of course your software does not use a frame buffer directly. Rather the process of getting
those bytes from the file into your eyeballs needs some 10 million lines of code for reading that compressed
image file such as JP(E?)G or PNG and then using some graphics library to send the bytes down the graphics
stack to the display compositor etc. until it gets to the graphics driver and from there finally to the display
device.

This is the process that will practically always go wrong one way or another.

When programming using OpenGL there are several points where we must carefully think how the gamma
encoding (sRGB data) interacts with the system (or rather is supposed to interact)

- Default frame buffer created by the window system
    - I.e. what is the encoding of the output buffer where the fragments are written for display
- OpenGL created frame buffer and their color attachment render targets (texture, render buffer)
- OpenGL texture objects used for sampling
- Shader uniforms that contain "color" data
- glClearColor
- glReadPixels

 **RENDERBUFFER FBO RENDER TARGET**
 todo

 **RGBA TEXTURE FBO RENDER TARGET**
 todo:

 **TEXTURE 2D**
 todo:

 **SHADER UNIFORMS**

```
uniform vec4 color0;
uniform vec4 color1;

void main()
{
   gl_FragColor = mix(color0, color1, 0.5);
}

```
In this above simple GLSL code we may or may not produce the correct output depending on what is in the
color0 and color1 uniforms.

If the data that we pass for 'color0' and 'color1' uniforms happens to be sRGB encoded then the mixing will
be incorrect since linearly interpolating sRGB encoded data will not yield correct results. Instead both
uniforms will need to be converted into linear values and then mixed.

The question is what is the right place to do this conversion?

- Program interface?
- Material property interface?
- Editor's color picker integration?

</details>


#### Gamma Correction Bugs

<details><summary>NVIDIA 530.41.03 ARCHLINUX 6.3.3</summary><br>
- Using GLX to create the GL rendering context.
- Trying to create the context using a non sRGB config fails
- It seems that sRGB property is not properly understood, i.e. when the shader writes
  fragment values the output is not correctly sRGB encoded. The result is visually too dark.

=> Looks like here sRGB correct rendering is not possible. The shader should manually write sRGB encoded values
   but if it does so then blending will most likely be broken and uses sRGB values.<br>
=> Best (least broken) result would be achieved by making sure that the shader uses linear values from vec4 uniforms
   and then sRGB encodes before writing to the gl_FragColor. RT color buffer blending will still be incorrect but at least
   what happens in the shader would be correct.

</details>

<details><summary>NVIDIA sRGB ES BUG</summary><br>
https://forums.developer.nvidia.com/t/gl-framebuffer-srgb-functions-incorrectly/34889/11

</details>


















