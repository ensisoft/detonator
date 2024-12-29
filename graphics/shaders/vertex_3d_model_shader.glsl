R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

// @attributes
in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;

// @uniforms
uniform mat4 kProjectionMatrix;
uniform mat4 kModelViewMatrix;

// @varyings
out vec2 vTexCoord;

void VertexShaderMain() {

    vTexCoord = aTexCoord;

    vec4 view_position = kModelViewMatrix * vec4(aPosition.xyz, 1.0);
    vs_out.view_position = view_position;
    vs_out.clip_position = kProjectionMatrix * view_position;
}

)CPP_RAW_STRING"
