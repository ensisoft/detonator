#version 100

precision mediump float;

uniform sampler2D kTexture;
uniform float kRenderPoints; // 1.0 when rendering points
uniform float kGamma;
uniform float kTimeSecs;
// blink text, on/off cycle length
uniform float kBlinkPeriod;
uniform vec4  kBaseColor;
uniform mat3  kDeviceTextureMatrix;
varying vec2  vTexCoord;

void main()
{
    // blink text, on/off cycle length
    if (kBlinkPeriod > 0.0) 
    {
        float s = step(kBlinkPeriod * 0.5, mod(kTimeSecs, kBlinkPeriod));
        if (s < 1.0)
            discard;
    }

    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    vec3 transformed_coords = kDeviceTextureMatrix * vec3(coords.xy, 1.0);
    coords = transformed_coords.xy;

    vec4 tex = texture2D(kTexture, coords);
    vec4 col = kBaseColor * tex.a; // use the texture as the alpha mask
    gl_FragColor = pow(col, vec4(kGamma, kGamma, kGamma, kGamma));
}
