R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

precision highp float;

// @uniforms
uniform uint kMaterialFlags;
uniform vec4 kDiffuseColor;
uniform vec4 kAmbientColor;
uniform vec4 kSpecularColor;
uniform float kSpecularExponent;

uniform sampler2D kDiffuseMap;
uniform sampler2D kSpecularMap;
uniform sampler2D kNormalMap;

uniform vec4 kDiffuseMapRect;
uniform vec4 kSpecularMapRect;
uniform vec4 kNormalMapRect;

uniform uint kMaterialMaps;

// @varyings
in vec2 vTexCoord;

bool TestMap(uint map) {
    return ((kMaterialMaps & map) == map);
}

void FragmentShaderMain() {

    vec4 ambient_color = kAmbientColor;
    vec4 diffuse_color = kDiffuseColor;
    vec4 specular_color = kSpecularColor;

    if (TestMap(BASIC_LIGHT_MATERIAL_DIFFUSE_MAP)) {
        vec2 diffuse_map_coords = vTexCoord * kDiffuseMapRect.zw + kDiffuseMapRect.xy;
        vec4 diffuse_map_sample = texture(kDiffuseMap, diffuse_map_coords);
        // using the color to modulate the texture samplling
        diffuse_color *= diffuse_map_sample;
        ambient_color *= diffuse_map_sample;
    }

    if (TestMap(BASIC_LIGHT_MATERIAL_SPECULAR_MAP)) {
        vec2 specular_map_coords = vTexCoord * kSpecularMapRect.zw + kSpecularMapRect.xy;
        vec4 specular_map_sample = texture(kSpecularMap, specular_map_coords);
        // using the color to modulate the texture sampling
        specular_color *= specular_map_sample;
    }

    if (TestMap(BASIC_LIGHT_MATERIAL_NORMAL_MAP)) {
        vec2 normal_map_coords = vTexCoord * kNormalMapRect.zw + kNormalMapRect.xy;
        vec4 normal_map_sample = texture(kNormalMap, normal_map_coords);
        fs_out.surface_normal = normalize(normal_map_sample.xyz * 2.0 - 1.0);
        fs_out.have_surface_normal = true;
    }

    fs_out.color = diffuse_color;
    fs_out.ambient_color = ambient_color;
    fs_out.diffuse_color = diffuse_color;
    fs_out.specular_color = specular_color;
    fs_out.specular_exponent = kSpecularExponent;
    fs_out.have_material_colors = true;
    fs_out.flags = kMaterialFlags;
}

)CPP_RAW_STRING"