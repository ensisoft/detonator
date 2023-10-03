
#version 100

precision highp float;

// build-in uniforms
// material time in seconds.
uniform float kTime;

// when rendering particles with points the material
// shader must sample gl_PointCoord for texture coordinates
// instead of the texcoord varying from the vertex shader.
// the value kRenderPoints will be set to 1.0 so for portability
// the material shader can do:
//   vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
uniform float kRenderPoints;

// custom uniforms that need to match the json description
uniform vec4 kBaseColor;

uniform sampler2D kTexture;

// varyings from vertex stage.
varying vec2 vTexCoord;

// per particle data.
// these are only written when the drawable is a particle engine
varying float vParticleAlpha;
// particle random value.
varying float vParticleRandomValue;
// normalized particle lifetime.
varying float vParticleTime;

float spiral(vec2 m) {
	float r = length(m);
	float a = atan(m.y, m.x);
	float v = sin(50.0 * (sqrt(r)-0.5*a + 0.05*kTime));
	return clamp(v, 0.0, 1.0);

}

void main() {

    float v = spiral(vTexCoord - vec2(0.5, 0.5));

    vec4 tex = texture2D(kTexture, vTexCoord);

    gl_FragColor.rgb = mix(kBaseColor.rgb, tex.rgb, 1.0-v);
    //gl_FragColor.rgb = mix(tex.rgb, kBaseColor.rgb, 1.0-v);
    gl_FragColor.a = 1.0;

    //vec4(tex.rgb * kBaseColor.rgb, v)
}
