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

// outgoing random value per drawable vertex.
// currently only provided by the particle engine
// in order to simulate some per particle feature
// in the material system.
varying float vRandomValue;

void main()
{
    // put the vertex from "model" space into same coordinate space
    // with view space. I.e. x grows left and y grows down
    // the model data is defined in the lower right quadrant in
    // NDC (normalied device coordinates) (x grows right to 1.0 and
    // y grows up to 1.0 to the top of the screen)
    vec4 vertex = vec4(aPosition.x, aPosition.y * -1.0, 1.0, 1.0);

    // texture coordinates to fragment shader.
    vTexCoord = aTexCoord;

    // todo: provide this if needed. currently only particle engins
    // support this, not other geomeries. for other drawables it 
    // could come from an uniform (applies to the shape as a whole)
    // or from the vertex data (applies to each vertex separately)
    vRandomValue = 0.0;

    // output position.
    gl_Position = kProjectionMatrix * kViewMatrix * vertex;
}
