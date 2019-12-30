#version 100

precision mediump float;

uniform sampler2D kTexture;
uniform float kRenderPoints; // 1.0 when rendering points
uniform float kGamma;
uniform vec4  kBaseColor;
uniform vec4  kTextureBox;
uniform mat3  kDeviceTextureMatrix;
varying vec2  vTexCoord;

void main()
{
    vec2 coords = mix(vTexCoord, gl_PointCoord, kRenderPoints);
    coords = vec2(kTextureBox.x + coords.x * kTextureBox.z,
                  kTextureBox.y + coords.y * kTextureBox.w);

    vec3 transformed_coords = kDeviceTextureMatrix * vec3(coords.xy, 1.0);
    coords = transformed_coords.xy;
    vec4 tex = texture2D(kTexture, coords);
    vec4 col = tex * kBaseColor;
    gl_FragColor = pow(col, vec4(kGamma, kGamma, kGamma, kGamma));
}

