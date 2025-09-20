R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// simple 3D vertex shader

#version 300 es

// @attributes
in vec3 aPosition;
in vec2 aTexCoord;
in vec3 aNormal;
in vec3 aTangent;
in vec3 aBitangent;

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

uniform uint kDrawableFlags;

// @varyings
out vec2 vTexCoord;

void VertexShaderMain() {
    vTexCoord = aTexCoord;

    if ((kDrawableFlags & DRAWABLE_FLAGS_FLIP_UV_HORIZONTALLY) == DRAWABLE_FLAGS_FLIP_UV_HORIZONTALLY) {
      vTexCoord.x = 1.0 - vTexCoord.x;
    }
    if ((kDrawableFlags & DRAWABLE_FLAGS_FLIP_UV_VERTICALLY) == DRAWABLE_FLAGS_FLIP_UV_VERTICALLY) {
      vTexCoord.y = 1.0 - vTexCoord.y;
    }

    mat4 model_inst_matrix = GetInstanceTransform();
    mat4 model_view_matrix = kModelViewMatrix * model_inst_matrix;
    vec4 view_position = model_view_matrix * vec4(aPosition.xyz, 1.0);

    vs_out.view_position = view_position;
    vs_out.clip_position = kProjectionMatrix * view_position;

    if (vs_out.need_tbn) {
      mat3 normal_matrix = mat3(transpose(inverse(model_view_matrix)));
      vs_out.view_normal    = normalize(normal_matrix * aNormal);
      vs_out.view_tangent   = normalize(normal_matrix * aTangent);
      vs_out.view_bitangent = normalize(normal_matrix * aBitangent);
      vs_out.have_tbn = true;
    }
}

void VertexShaderShadowPass() {
  mat4 model_inst_matrix = GetInstanceTransform();
  mat4 model_view_matrix = kModelViewMatrix * model_inst_matrix;
  vec4 view_position = model_view_matrix * vec4(aPosition.xyz, 1.0);
  vs_out.clip_position = kProjectionMatrix * view_position;
}

)CPP_RAW_STRING"
