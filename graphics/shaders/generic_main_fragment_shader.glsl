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
    uint flags;
} fs_out;

// @uniforms
#if defined(ENABLE_BLOOM_OUT)
  uniform float kBloomThreshold;
  uniform vec4  kBloomColor;
#endif

// @varyings

#if defined(ENABLE_BASIC_LIGHT) || defined(ENABLE_BASIC_FOG)
  in vec3 vertexViewPosition;
#endif

#if defined(ENABLE_BASIC_LIGHT)
  in vec3 vertexViewNormal;
  in mat3 TBN;
#endif

// @out

#if defined(ENABLE_COLOR_OUT)
  // this is the default color buffer out color.
  layout (location=0) out vec4 fragOutColor0;
#endif

#if defined(ENABLE_BLOOM_OUT)
  layout (location=1) out vec4 fragOutColor1;
#endif

// @code

#if defined(ENABLE_BLOOM_OUT)
vec4 Bloom(vec4 color) {
    float brightness = dot(color.rgb, kBloomColor.rgb); //vec3(0.2126, 0.7252, 0.0722));
    if (brightness > kBloomThreshold)
       return color;
    return vec4(0.0, 0.0, 0.0, 0.0);
}
#endif

// $CUSTOM_FRAGMENT_MAIN

void main() {
    fs_out.have_material_colors = false;
    fs_out.have_surface_normal = false;
    vec4 out_color;

#if defined(FRAGMENT_SHADER_MAIN_RENDER_PASS)
    #if defined(CUSTOM_FRAGMENT_MAIN)
        CustomFragmentShaderMain();
    #else
        FragmentShaderMain();
    #endif

    out_color = fs_out.color;

    #if defined(ENABLE_BASIC_LIGHT)
        if ((fs_out.flags & MATERIAL_FLAGS_ENABLE_LIGHT) == MATERIAL_FLAGS_ENABLE_LIGHT) {
            out_color = ComputeBasicLight();
        }
    #endif

    #if defined(ENABLE_BASIC_FOG)
        out_color = ComputeBasicFog(out_color);
    #endif

#elif defined(FRAGMENT_SHADER_STENCIL_MASK_RENDER_PASS)
    // todo: maybe have separate stencil function here...
    #if defined(CUSTOM_FRAGMENT_MAIN)
        CustomFragmentShaderMain();
    #else
        FragmentShaderMain();
    #endif
    out_color = fs_out.color;

#elif defined(FRAGMENT_SHADER_SHADOW_MAP_RENDER_PASS)
    // do nothing for now!
    return;
#endif

    #if defined(ENABLE_COLOR_OUT)
        fragOutColor0 = sRGB_encode(out_color);
    #endif

    #if defined(ENABLE_BLOOM_OUT)
        if ((fs_out.flags & MATERIAL_FLAGS_ENABLE_BLOOM) == MATERIAL_FLAGS_ENABLE_BLOOM) {
            vec4 bloom = Bloom(out_color);
            fragOutColor1 = sRGB_encode(bloom);
        }
    #endif
}

)CPP_RAW_STRING"