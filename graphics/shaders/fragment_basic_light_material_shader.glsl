R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

precision highp float;

// @uniforms
uniform vec4 kDiffuseColor;
uniform vec4 kAmbientColor;
uniform vec4 kSpecularColor;
uniform float kSpecularExponent;

uniform sampler2D kDiffuseMap;
uniform sampler2D kSpecularMap;

uniform vec4 kDiffuseMapRect;
uniform vec4 kSpecularMapRect;

uniform int kMaterialMaps;

// @varyings
in vec2 vTexCoord;

bool TestMap(int map) {
    return ((kMaterialMaps & map) == map);
}

void FragmentShaderMain() {

    vec4 ambient_color = kAmbientColor;
    vec4 diffuse_color = kDiffuseColor;
    vec4 specular_color = kSpecularColor;

    if (TestMap(LIGHT_DIFFUSE_MAP)) {
        vec2 diffuse_map_coords = vTexCoord * kDiffuseMapRect.zw + kDiffuseMapRect.xy;
        vec4 diffuse_map_sample = texture(kDiffuseMap, diffuse_map_coords);
        // using the color to modulate the texture samplling
        diffuse_color *= diffuse_map_sample;
        ambient_color *= diffuse_map_sample;
    }

    if (TestMap(LIGHT_SPECULAR_MAP)) {
        vec2 specular_map_coords = vTexCoord * kSpecularMapRect.zw + kSpecularMapRect.xy;
        vec4 specular_map_sample = texture(kSpecularMap, specular_map_coords);
        // using the color to modulate the texture sampling
        specular_color *= specular_map_sample;
    }

    fs_out.color = kDiffuseColor;
    fs_out.ambient_color = ambient_color;
    fs_out.diffuse_color = diffuse_color;
    fs_out.specular_color = specular_color;
    fs_out.specular_exponent = kSpecularExponent;
    fs_out.use_light = true;
}

)CPP_RAW_STRING"