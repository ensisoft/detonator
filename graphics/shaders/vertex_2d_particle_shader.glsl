R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// particle vertex shader

#version 300 es

// @attributes

// particle position in the particle simulation space.
// could be "local" or "global". the model matrix must
// be adjusted accordingly.
in vec2 aPosition;
// per particle data:
// x = particle size to point rasterizer (gl_PointSize)
// y = particle random value
// z = particle alpha / transparency
// w = particle lifetime as a fraction of maximum lifetime
in vec4 aData;

// particle direction vector
in vec2 aDirection;

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
out float vParticleRandomValue;
out float vParticleAlpha;
out float vParticleTime;
out float vParticleAngle;

void VertexShaderMain() {
    vec4 vertex = vec4(aPosition.x, aPosition.y, 0.0, 1.0);

    // angle of the direction vector relative to the x axis
    float cosine = dot(vec2(1.0, 0.0), normalize(aDirection));

    float angle = 0.0;
    if (aDirection.y < 0.0)
        angle = -acos(cosine);
    else angle = acos(cosine);

    vParticleAngle       = angle;
    vParticleRandomValue = aData.y;
    vParticleAlpha       = aData.z;
    vParticleTime        = aData.w;

    // base vertex shader
    mat4 instance_matrix = GetInstanceTransform();

    vec4 view_position = kModelViewMatrix * instance_matrix * vertex;

    vs_out.view_position = view_position;
    vs_out.clip_position = kProjectionMatrix * view_position;
    vs_out.point_size    = aData.x;
}

)CPP_RAW_STRING"