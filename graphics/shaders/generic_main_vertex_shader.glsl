R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic vertex shader main function that works
// with the actual per drawable vertex shaders.

#version 300 es

struct VS_OUT {
    // vertex position in clip space (after projection transformation)
    vec4 clip_position;
    // vertx position in eye coordinates (after camera/view transformation)
    vec4 view_position;
    // view space surface normal vector
    vec3 view_normal;
    // view space surface tangent vector
    vec3 view_tangent;
    // view space surface bi-tangent vector
    vec3 view_bitangent;
    // point size for GL_POINTS rasterization.
    float point_size;

    bool need_tbn;
    bool have_tbn;
} vs_out;

// @varyings

#if defined(ENABLE_BASIC_LIGHT) || defined(ENABLE_BASIC_FOG)
  out vec3 vertexViewPosition;
#endif

#if defined(ENABLE_BASIC_LIGHT)
  out vec3 vertexViewNormal;
  // tangent, bitangent, normal matrix for normal mapping
  out mat3 TBN;
#endif

void main() {
    vs_out.have_tbn = false;
    vs_out.need_tbn = false;

#if defined(VERTEX_SHADER_MAIN_RENDER_PASS)
    #if defined(ENABLE_BASIC_LIGHT)
        vs_out.need_tbn = true;
    #endif

    VertexShaderMain();

    #if defined(ENABLE_BASIC_LIGHT) || defined(ENABLE_BASIC_FOG)
        vertexViewPosition = vs_out.view_position.xyz;
    #endif

    #if defined(ENABLE_BASIC_LIGHT)
        if (vs_out.have_tbn) {
            vertexViewNormal = vs_out.view_normal;
            TBN = mat3(vs_out.view_tangent,
                       vs_out.view_bitangent,
                       vs_out.view_normal);
        }
    #endif
#elif defined(VERTEX_SHADER_STENCIL_MASK_RENDER_PASS)
    // we support stencil masking based on texture alpha masks
    // by doing fragment discard. so in order to support that
    // properly we cannot just do a simple vertex shader but
    // must transform texture coordinates, sample texture etc.
    VertexShaderMain();
#elif defined(VERTEX_SHADER_SHADOW_MAP_RENDER_PASS)
    VertexShaderShadowPass();
#endif

    gl_PointSize = vs_out.point_size;
    gl_Position  = vs_out.clip_position;
}

)CPP_RAW_STRING"