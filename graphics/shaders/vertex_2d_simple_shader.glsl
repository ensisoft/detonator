R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// particle vertex shader

#version 300 es

// @attributes

in vec2 aPosition;
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

// @uniforms
uniform mat4 kProjectionMatrix;
uniform mat4 kModelViewMatrix;

// @varyings
out vec2 vTexCoord;

void VertexShaderMain() {
    vec4 vertex  = vec4(aPosition.x, aPosition.y * -1.0, 0.0, 1.0);
    vTexCoord    = aTexCoord;

    mat4 instance_matrix = GetInstanceTransform();

    vec4 view_position = kModelViewMatrix * instance_matrix * vertex;
    vs_out.view_position = view_position;
    vs_out.clip_position = kProjectionMatrix * view_position;
}

)CPP_RAW_STRING"