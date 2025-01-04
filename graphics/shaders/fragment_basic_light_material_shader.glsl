R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

precision highp float;

// @uniforms
uniform vec4 kDiffuseColor;
uniform vec4 kAmbientColor;
uniform vec4 kSpecularColor;
uniform float kSpecularExponent;

void FragmentShaderMain() {

    fs_out.color = kDiffuseColor;
    fs_out.diffuse_color = kDiffuseColor;
    fs_out.ambient_color = kAmbientColor;
    fs_out.specular_color = kSpecularColor;
    fs_out.specular_exponent = kSpecularExponent;
    fs_out.use_light = true;
}

)CPP_RAW_STRING"