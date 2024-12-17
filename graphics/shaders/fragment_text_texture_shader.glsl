R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// text shader for text that is prerendered in a texture.

uniform sampler2D kTexture;

in vec2 vTexCoord;

void FragmentShaderMain() {
    mat3 flip = mat3(vec3(1.0,  0.0, 0.0),
                     vec3(0.0, -1.0, 0.0),
                     vec3(0.0,  1.0, 0.0));
    vec3 tex = flip * vec3(vTexCoord.xy, 1.0);
    vec4 color = texture(kTexture, tex.xy);
    fs_out.color = color;
}

)CPP_RAW_STRING"