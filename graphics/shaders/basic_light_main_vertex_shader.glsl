R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic vertex shader main function that works
// with the actual per drawable vertex shaders.

#version 300 es

struct VS_OUT {
    vec4 clip_position;
    vec4 view_position;
    vec3 view_normal;
    float point_size;
} vs_out;

out vec3 vertexViewPosition;
out vec3 vertexViewNormal;

void main() {
    VertexShaderMain();

    gl_PointSize = vs_out.point_size;
    gl_Position  = vs_out.clip_position;

    vertexViewPosition = vs_out.view_position.xyz;
    vertexViewNormal = normalize(vs_out.view_normal);
}

)CPP_RAW_STRING"