// Built-in (from material system) uniforms
// -------------------------------------------------
// the monotonic material instance time in seconds.
uniform float kTime;
// Flag to control whether to use point coordinates (gl_PointCoord)
// or  normal vertex coordinates from vertex shader.
uniform float kRenderPoints;


// Custom uniforms
// -------------------------------------------------
// the mask texture for alpha compositing.
uniform sampler2D kMask;
// the sub-rectangle in the texture from which to sample.
uniform vec4 kMaskRect;
// particle start color
uniform vec4 kStartColor;
// Particle end color when time is 1.0
uniform vec4 kEndColor;
// Velocity of texture coordinate rotation about the z axis
// in radians per second.
uniform float kRotationalVelocity;
// Flag to control whether or not to rotate texture coordinates.
// Set to 1.0 for rotation and 0.0 for no rotation.
uniform float kRotate;

uniform float kBaseRotation;

// Vertex input
// ---------------------------------------------------
// Vertex texture coordinates.
varying vec2 vTexCoord;
// Normalized lifetime of the particle
varying float vParticleTime;
// Particle alpha value.
varying float vParticleAlpha;
// Particle random value. [0.0, 1.0]
varying float vParticleRandomValue;

void FragmentShaderMain()
{
    float angle = kTime * kRotationalVelocity * vParticleRandomValue + kBaseRotation * (vParticleRandomValue - 0.5);

    // either read varying texture coords from the vertex shader
    // or use gl_PointCoord which when rendering GL_POINTS
    vec2 coord = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    // discard fragments if the rotated texture coordinates would
    // fall outside of the 0.0-1.0 range.
    float len = length(coord - vec2(0.5, 0.5)) * kRotate;
    if (len > 0.5)
        discard;

    // rotate texture coords.
    if (kRotate >= 1.0)
    {
        coord -= vec2(0.5, 0.5);
        coord  = mat2(cos(angle), -sin(angle),
                      sin(angle),  cos(angle)) * coord;
        coord += vec2(0.5, 0.5);
    }

    // apply texture rect transform to get the final coordinates
    // for sampling the texture. this transformation allows for
    // texture packing to be done.
    vec2 texture_trans = kMaskRect.xy;
    vec2 texture_scale = kMaskRect.zw;
    coord = coord * texture_scale + texture_trans;

    float alpha = texture2D(kMask,  coord).r;

    vec4 color = mix(kStartColor, kEndColor, vParticleTime);

    // blend with straight alpha and with transparent surface
    fs_out.color.rgb = color.rgb;
    fs_out.color.a   = (alpha * vParticleAlpha * color.a);
}

