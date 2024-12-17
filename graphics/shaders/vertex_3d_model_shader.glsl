R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

#version 300 es

in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;

uniform mat4 kProjectionMatrix;
uniform mat4 kModelViewMatrix;

out vec2 vTexCoord;
out float vParticleRandomValue;
out float vParticleAlpha;
out float vParticleTime;
out float vParticleAngle;
out vec2 vTileData;

void VertexShaderMain() {

    vTexCoord = aTexCoord;
    // dummy out
    vParticleRandomValue = 0.0;
    vParticleAlpha       = 1.0;
    vParticleTime        = 0.0;
    vParticleAngle       = 0.0;

    vec4 view_position = kModelViewMatrix * vec4(aPosition.xyz, 1.0);
    vs_out.view_position = view_position;
    vs_out.clip_position = kProjectionMatrix * view_position;
}

)CPP_RAW_STRING"
