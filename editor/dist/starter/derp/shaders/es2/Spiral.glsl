#version 300 es

// build-in uniforms
// material time in seconds.
uniform float kTime;

// custom uniforms that need to match the json description
uniform vec4 kBaseColor;

uniform sampler2D kTexture;

// varyings from vertex stage.
varying vec2 vTexCoord;

float spiral(vec2 m) {
	float r = length(m);
	float a = atan(m.y, m.x);
	float v = sin(50.0 * (sqrt(r)-0.5*a + 0.05*kTime));
	return clamp(v, 0.0, 1.0);

}

void FragmentShaderMain() {

    float v = spiral(vTexCoord - vec2(0.5, 0.5));

    vec4 tex = texture(kTexture, vTexCoord);

    fs_out.color.rgb = mix(kBaseColor.rgb, tex.rgb, 1.0-v);
    //gl_FragColor.rgb = mix(tex.rgb, kBaseColor.rgb, 1.0-v);
    fs_out.color.a = 1.0;

    //vec4(tex.rgb * kBaseColor.rgb, v)
}
