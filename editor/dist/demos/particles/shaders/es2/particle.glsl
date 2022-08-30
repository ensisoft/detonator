#version 100

precision highp float;

uniform float kRuntime;
uniform float kRenderPoints;

uniform sampler2D kMask;
uniform vec4 kStartColor;
uniform vec4 kEndColor;
uniform float kDuration;

varying vec2 vTexCoord;
// normalized lifetime of the particle
varying float vParticleTime;
// particle alpha
varying float vParticleAlpha;

void main()
{
    vec2 coord = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    float alpha = texture2D(kMask,  coord).r;

    gl_FragColor = mix(kStartColor, kEndColor, vParticleTime);
    gl_FragColor.rgb *= (alpha * vParticleAlpha);
}