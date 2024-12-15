R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// simple 3D vertex shader

#version 300 es

in vec3 aPosition;
in vec2 aTexCoord;

// these should be in the base_vertex_shader.glsl but alas
// it seems that if these are defined *before* the non-instanced
// attributes we have some weird problems, the graphics_test app
// stops rendering after instanced particle rendering is done.
// smells like a driver bug.
#ifdef INSTANCED_DRAW
// the per instance model-to-world mat4 is broken
// down into 4 vectors.
  in vec4 iaModelVectorX;
  in vec4 iaModelVectorY;
  in vec4 iaModelVectorZ;
  in vec4 iaModelVectorW;
#endif

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
    vTileData            = vec2(0.0, 0.0);

    mat4 instance_matrix = GetInstanceTransform();

    gl_Position = kProjectionMatrix * kModelViewMatrix * instance_matrix * vec4(aPosition.xyz, 1.0);

}

)CPP_RAW_STRING"
