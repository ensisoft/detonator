R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic fragment shader main that works with the
// actual material fragment shaders

#version 300 es

// leave precision undefined here, material shader can
// define that since those can be user specified.
// precision highp float;

// @types

// this is what the material shader fills.
struct FS_OUT {
    vec4 color;

    vec4 ambient_color;
    vec4 diffuse_color;
    vec4 specular_color;
    vec3 surface_normal;
    float specular_exponent; // aka shininess
    bool have_material_colors;
    bool have_surface_normal;
} fs_out;

// @varyings

#if defined(ENABLE_BASIC_LIGHT) || defined(ENABLE_BASIC_FOG)
  in vec3 vertexViewPosition;
#endif

#if defined(ENABLE_BASIC_LIGHT)
  in vec3 vertexViewNormal;
  in mat3 TBN;
#endif

// @out

// this is the default color buffer out color.
layout (location=0) out vec4 fragOutColor;

// @code

void main() {
    fs_out.have_material_colors = false;
    fs_out.have_surface_normal = false;
    FragmentShaderMain();

    vec4 out_color = fs_out.color;

    #ifdef ENABLE_BASIC_LIGHT
        out_color = ComputeBasicLight();
    #endif

    #ifdef ENABLE_BASIC_FOG
        out_color = ComputeBasicFog(out_color);
    #endif

    fragOutColor = sRGB_encode(out_color);
}

)CPP_RAW_STRING"