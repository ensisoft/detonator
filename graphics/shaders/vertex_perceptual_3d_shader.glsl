R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Perceptual 3D shader that is used to render 2D content
// that is perceptually 3D, such as isometric tiles.

#version 300 es

// @attributes
in vec2 aPosition;
in vec2 aTexCoord;
in vec3 aLocalOffset;
in vec3 aWorldNormal;

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
uniform mat4 kAxonometricModelViewMatrix;
uniform uint kDrawableFlags;

#ifdef CUSTOM_VERTEX_TRANSFORM
  uniform float kTime;
  uniform float kRandom;
#endif

// @varyings
out vec2 vTexCoord;

// @code
struct VertexData {
  vec4 render_vertex;
  vec3 render_normal;
  vec3 local_offset;
  vec3 world_normal;
  vec2 texcoord;
};

#ifdef CUSTOM_VERTEX_TRANSFORM
// $CUSTOM_VERTEX_TRANSFORM
#endif

void VertexShaderMain() {

    // see the 2D simple vertex shader about the reasons
    // why the Y axis is flipped.

    VertexData vs;
    vs.render_vertex = vec4(aPosition.x, -aPosition.y, 0.0, 1.0);
    vs.render_normal = vec3(0.0, 0.0, -1.0);
    vs.local_offset = aLocalOffset;
    vs.world_normal = aWorldNormal;
    vs.texcoord = aTexCoord;

    // doesn't make much sense but lets implement this for the
    // sake of consistency.
    if ((kDrawableFlags & DRAWABLE_FLAGS_FLIP_UV_HORIZONTALLY) == DRAWABLE_FLAGS_FLIP_UV_HORIZONTALLY) {
      vs.texcoord.x = 1.0 - vs.texcoord.x;
    }
    if ((kDrawableFlags & DRAWABLE_FLAGS_FLIP_UV_VERTICALLY) == DRAWABLE_FLAGS_FLIP_UV_VERTICALLY) {
      vs.texcoord.y = 1.0 - vs.texcoord.y;
    }

    #ifdef CUSTOM_VERTEX_TRANSFORM
        CustomVertexTransform(vs);
    #endif

    vTexCoord = vs.texcoord;


    if ((kDrawableFlags & DRAWABLE_FLAGS_ENABLE_PERCEPTUAL_3D) == DRAWABLE_FLAGS_ENABLE_PERCEPTUAL_3D) {
      mat3 normal_matrix = mat3(transpose(inverse(kModelViewMatrix)));
      vec4 view_position = kModelViewMatrix * vec4(aLocalOffset, 1.0);
      vec3 view_normal   = normal_matrix * aWorldNormal;

      vs_out.view_normal   = normalize(view_normal);
      vs_out.view_position = view_position;
      vs_out.clip_position = kProjectionMatrix * view_position;
    } else {
      mat3 normal_matrix = mat3(transpose(inverse(kAxonometricModelViewMatrix)));
      vec4 view_position = kAxonometricModelViewMatrix * vec4(aLocalOffset, 1.0);
      vec3 view_normal   = normal_matrix * aWorldNormal;

      vs_out.clip_position = kProjectionMatrix * kModelViewMatrix * vs.render_vertex;
      vs_out.view_position = view_position;
      vs_out.view_normal   = normalize(view_normal);
    }

    // this is a lie, we don't have tangent/bitangent
    // todo: maybe separate these?
    vs_out.have_tbn = true;
}

)CPP_RAW_STRING"
