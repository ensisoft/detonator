#version 100

// incoming vertex attributes. 
// position is the vertex position in model space
attribute vec2 aPosition;
// texture coordinate for the vertex
attribute vec2 aTexCoord;

// transform vertex from view space into NDC
// we're using orthographic projection
uniform mat4 kProjectionMatrix;

// transform vertex from model space into viewspace
uniform mat4 kViewMatrix;

// outgoing texture coordinate
varying vec2 vTexCoord;

void main()
{
    // put the vertex from "model" space into same coordinate space
    // with view space. I.e. x grows left and y grows down 
    // the model data is defined in the lower right quadrant in
    // NDC (normalied device coordinates) (x grows right to 1.0 and 
    // y grows up to 1.0 to the top of the screen)
    vec4 vertex = vec4(aPosition.x, aPosition.y * -1.0, 1.0, 1.0);

    vTexCoord = aTexCoord;

    gl_Position = kProjectionMatrix * kViewMatrix * vertex;

    // we're abusing the vertex type here and packing
    // the pointsize in the texture coordinate field.
    // if the rendering is points the texture coord
    // is irrelevant since the fragment shader needs to
    // sample from gl_PointCoord
    gl_PointSize = vTexCoord.x;
}
