#version 300 es
// warning. do not delete the below line.
// MAPI=1

//// material uniforms ////

// material instance time in seconds.
uniform float kTime;

// The current texture to sample from.
uniform sampler2D kTexture;

// Texture box (sub-rectangle) from which to read
// the texture. This is a packed uniform so that xy
// components define the texture box translation
// and zw define the size of the box.
// You can use these data to transform the incoming
// texture coordinates so that the sampling happens
// from the desired area of the texture (the texture rect)
uniform vec4 kTextureBox;

// texture coordinates
in vec2 vTexCoord;

vec2 Motion(vec2 st)
{
    float angle = 2.0*3.1417 * sin(kTime);

    st -= vec2(0.5, 0.5);
    st = mat2(cos(angle), -sin(angle),
              sin(angle), cos(angle)) * st;
    st += vec2(0.5, 0.5);

    st.x = st.x + sin(kTime*0.1);
    st.y = st.y + cos(kTime*0.5);
    return st;
}

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

void FragmentShaderMain()
{
    // your code here
    float sine = sin(kTime*0.78 + sin(vTexCoord.x * 4.5)) * 0.5 + 0.5;

    float color_modulator = sine;

    vec4 src_color_a = vec4(1.0, 0.3, 0.0, 1.0);
    vec4 src_color_b = vec4(1.0, 0.8, 0.0, 1.0);
    vec4 src_hilite_color_a = vec4(0.3);
    vec4 src_hilite_color_b = vec4(0.4);

    vec4 src_text_color;

    float hilight_width = 0.28;
    float hilite_alpha = smoothstep(color_modulator-hilight_width, color_modulator, vTexCoord.y) -
                  smoothstep(color_modulator, color_modulator+hilight_width, vTexCoord.y);

    if (vTexCoord.y > color_modulator)
    {
        src_text_color = src_color_b + src_hilite_color_b * hilite_alpha;
    }
    else
    {
        src_text_color = src_color_a + src_hilite_color_a * hilite_alpha;
    }

    vec2 texture_box_scale = kTextureBox.zw;
    vec2 texture_box_translate = kTextureBox.xy;
    vec2 texture_coord = vTexCoord * texture_box_scale + texture_box_translate;

    float color_sample_alpha = texture(kTexture, texture_coord).a;
    float shadow_sample_alpha = texture(kTexture, texture_coord - vec2(0.03, 0.04)).a;

    vec4 text_color   = vec4(src_text_color.rgb, color_sample_alpha);
    vec4 shadow_color = vec4(0.01, 0.01, 0.01, shadow_sample_alpha);

    if (text_color.a > 0.0)
        fs_out.color = text_color;
    else fs_out.color = shadow_color;
}
