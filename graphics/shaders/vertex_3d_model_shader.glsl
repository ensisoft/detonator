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

    gl_Position = kProjectionMatrix * kModelViewMatrix * vec4(aPosition.xyz, 1.0);

}

)CPP_RAW_STRING"
