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
    // point size for GL_POINTS rasterization.
    float point_size;
} vs_out;

void main() {
    VertexShaderMain();

    gl_PointSize = vs_out.point_size;
    gl_Position  = vs_out.clip_position;
}

)CPP_RAW_STRING"