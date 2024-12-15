R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic fragment shader main that works with the
// actual material fragment shaders

#version 300 es

// leave precision undefined here, material shader can
// define that since those can be user specified.
// precision highp float;

// this is the default color buffer out color.
layout (location=0) out vec4 fragOutColor;

// this is what the material shader fills.
struct FS_OUT {
    vec4 color;
} fs_out;

void main() {
    FragmentShaderMain();

    fragOutColor = sRGB_encode(fs_out.color);
}

)CPP_RAW_STRING"