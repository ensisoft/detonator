R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic vertex shader main function that works
// with the actual per drawable vertex shaders.

#version 300 es

struct VS_OUT {
    vec4 clip_position;
    vec4 view_position;
    vec3 view_normal;
    vec3 view_tangent;
    vec3 view_bitangent;
    float point_size;

    bool need_tbn;
    bool have_tbn;
} vs_out;

out vec3 vertexViewPosition;
out vec3 vertexViewNormal;
// tangent, bitangent, normal matrix for normal mapping
out mat3 TBN;

void main() {
    vs_out.have_tbn = false;
    vs_out.need_tbn = true;
    VertexShaderMain();

    vertexViewPosition = vs_out.view_position.xyz;

    if (vs_out.have_tbn) {
        vertexViewNormal = vs_out.view_normal;

        TBN = mat3(vs_out.view_tangent,
                   vs_out.view_bitangent,
                   vs_out.view_normal);
    }

    gl_PointSize = vs_out.point_size;
    gl_Position  = vs_out.clip_position;
}

)CPP_RAW_STRING"