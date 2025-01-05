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
    // view space surface tagent vector
    vec3 view_tangent;
    // view space surface bi-tangent vector
    vec3 view_bitangent;
    // point size for GL_POINTS rasterization.
    float point_size;

    bool need_tbn;
    bool have_tbn;
} vs_out;

void main() {
    vs_out.have_tbn = false;
    vs_out.need_tbn = false;
    VertexShaderMain();

    gl_PointSize = vs_out.point_size;
    gl_Position  = vs_out.clip_position;
}

)CPP_RAW_STRING"