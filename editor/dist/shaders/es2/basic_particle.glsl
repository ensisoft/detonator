// Built-in (from material system) uniforms
// -------------------------------------------------
// the monotonic material instance time in seconds.
uniform float kTime;
// Flag to control whether to use point coordinates (gl_PointCoord)
// or  normal vertex coordinates from vertex shader.
uniform float kRenderPoints;

// Surface type enum
uniform int kSurfaceType;

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

// if the rotation is random then the rotation value
// is texture coordinate rotation velocity (about the z axis)
// in radians per second.
// if the rotation is based on particle direction then this
// is the base rotation onto which the particle direction
// rotation angle is added
uniform float kRotationValue;
// Flag to control whether or not to rotate texture coordinates.
uniform int kRotate;

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
// Angle of the particle's direction vector
// relative to the x axis
varying float vParticleAngle;

#define PI 3.1415926
#define ROTATE_OFF 0
#define ROTATE_RANDOM 1
#define ROTATE_USE_PARTICLE_ANGLE 2
#define ROTATE_USE_PARTICLE_ANGLE_AND_BASE 3

void FragmentShaderMain()
{
    float angle = 0.0;

    if (kRotate == ROTATE_RANDOM)
    {
        float base_rotation = vParticleRandomValue * PI * 2.0;
        float time_rotation = kRotationValue * (vParticleRandomValue-0.5) * kTime;
        angle = base_rotation + time_rotation;
    }
    else if (kRotate == ROTATE_USE_PARTICLE_ANGLE)
    {
        angle = vParticleAngle;
    }
    else if (kRotate == ROTATE_USE_PARTICLE_ANGLE_AND_BASE)
    {
        float base_rotation = kRotationValue * PI * 2.0;
        angle = base_rotation + vParticleAngle;
    }

    // either read varying texture coords from the vertex shader
    // or use gl_PointCoord which when rendering GL_POINTS
    vec2 coord = mix(vTexCoord, gl_PointCoord, kRenderPoints);

    // rotate texture coords
    if (kRotate != ROTATE_OFF)
    {
        // discard fragments if the rotated texture coordinates would
        // fall outside of the 0.0-1.0 range.
        float len = length(coord - vec2(0.5, 0.5));
        if (len > 0.5)
            discard;

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

    if (kSurfaceType == MATERIAL_SURFACE_TYPE_TRANSPARENT)
    {
        // blend with straight alpha and with transparent surface
        // alpha is the particle transparency value
        fs_out.color.rgb = color.rgb;
        fs_out.color.a   = (alpha * vParticleAlpha * color.a);
    }
    else if (kSurfaceType == MATERIAL_SURFACE_TYPE_EMISSIVE)
    {
        // not blending transparently, alpha does not control
        // transparency but modulates the intensity of the color
        fs_out.color.rgb = color.rgb * alpha * vParticleAlpha;
        fs_out.color.a = 1.0;
    }
    else
    {
        fs_out.color.rgb = color.rgb;
    }
}

