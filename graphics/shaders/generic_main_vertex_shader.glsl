R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic vertex shader main function that works
// with the actual per drawable vertex shaders.

#version 300 es

struct VS_OUT {
    vec4 position;
} vs_out;

void main() {
    VertexShaderMain();
}

)CPP_RAW_STRING"