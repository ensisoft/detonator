#version 300 es

uniform uint kMaterialFlags;

uniform float kTime;

uniform vec4 kBaseColor;
uniform vec4 kShadowColor;

uniform sampler2D kLetter;
uniform sampler2D kMask;

in vec2 vTexCoord;

vec4 BlendTransparency(vec4 srcColor, vec4 dstColor) {
    // Compute the alpha blending factors
    float srcAlpha = srcColor.a;
    float dstAlpha = 1.0 - srcAlpha;

    // Blend the source and destination colors
    vec3 blendedColor = srcColor.rgb * srcAlpha + dstColor.rgb * dstAlpha;

    // Compute the resulting alpha
    float blendedAlpha = srcColor.a + dstColor.a * (1.0 - srcColor.a);

    // Return the blended color with the blended alpha
    return vec4(blendedColor, blendedAlpha);
}


void FragmentShaderMain() {

    vec2 texture_coords = vTexCoord;

    vec4 shadow_color = vec4(0.0);
    vec4 base_color = vec4(0.0);

    if ((texture_coords.x > 0.2) && (texture_coords.y > 0.2)) {
        float x = (texture_coords.x - 0.2) / 0.8;
        float y = (texture_coords.y - 0.2) / 0.8;
        if (x < 1.0 && y < 1.0) {
            float mask_sample = texture(kMask, vec2(x, y)).a;
            shadow_color = vec4(kShadowColor.rgb, mask_sample);
        }
    }

    if ((texture_coords.x > 0.05) && (texture_coords.y > 0.05)) {
        float x = (texture_coords.x - 0.05) / 0.9;
        float y = (texture_coords.y - 0.05) / 0.9;
        if (x < 1.0 && y < 1.0) {
            vec4 texture_sample = texture(kLetter, vec2(x, y));
            float mask_sample = texture(kMask, vec2(x, y)).a;
            base_color = vec4(texture_sample.rgb * kBaseColor.rgb, mask_sample);
        }
    }

    vec4 color = vec4(0.0);
    color = BlendTransparency(shadow_color, color);
    color = BlendTransparency(base_color, color);
    fs_out.color = color;
    fs_out.flags = kMaterialFlags;
}