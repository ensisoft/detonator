# DETONATOR 2D 💥💣

## Graphics Subsystem

todo:

This covers the graphics library in graphics/

Main classes are materials and drawables.

### Materials


### Drawables

### Gamma (in)Correction

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

#### Historical Display Device Gamma Encoding

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


#### In Real Life

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

#### OpenGL ES2

##### DEFAULT FRAME BUFFER
ES2 spec offers no explicit method for the OpenGL ES2 API to control the sRGB setting. Instead the spec
defers this decision to EGL. Of course EGL is not the only way to create OpenGL ES2 rendering contexts
since also WGL and GLX (and AGL) can be used with the right extensions.

<i>
The effects of GL commands on the window-system-provided framebuffer are
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
conjunction with different native platform window systems.
</i>

The EGL_KHR_gl_colorspace extension for asking for sRGB enabled window system
render targets then goes on to say this:

<i>
Only OpenGL and OpenGL ES contexts which support sRGB rendering must respect 
requests for EGL_GL_COLORSPACE_SRGB_KHR, and only to sRGB formats supported
by the context (normally just SRGB8). Older versions not supporting sRGB rendering
will ignore this surface attribute.
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

<strong>GL_FRAMEBUFFER_SRGB doesn't exist in GL ES2.</strong>

#### NVIDIA 530.41.03 ARCHLINUX 6.3.3

- Using GLX to create the GL rendering context.
- Trying to create the context using a non sRGB config fails 
- It seems that sRGB property is not properly understood, i.e. when the shader writes 
  fragment values the output is not correctly sRGB encoded. The result is visually too dark.

=> Looks like here sRGB correct rendering is not possible. The shader should manually write sRGB encoded values
   but if it does so then blending will most likely be broken and uses sRGB values.<br>
=> Best (least broken) result would be achieved by making sure that the shader uses linear values from vec4 uniforms
   and then sRGB encodes before writing to the gl_FragColor. RT color buffer blending will still be incorrect but at least
   what happens in the shader would be correct.


##### RENDERBUFFER FBO RENDER TARGET
todo:

##### RGBA TEXTURE FBO RENDER TARGET
todo:

##### TEXTURE 2D
todo:

##### SHADER UNIFORMS

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


#### WebGL 1

LOL !  

#### OpenGL ES3
todo

#### WebGL 2
todo

NVIDIA sRGB ES BUG

https://forums.developer.nvidia.com/t/gl-framebuffer-srgb-functions-incorrectly/34889/11




