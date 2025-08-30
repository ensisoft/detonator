R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// particle vertex shader

#version 300 es

// @attributes

in vec2 aPosition;
in vec2 aTexCoord;

#ifdef USE_EFFECTS_MESH
in vec4 aEffectShardData;
#endif

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

#ifdef CUSTOM_VERTEX_TRANSFORM
  uniform float kTime;
  uniform float kRandom;
#endif

#ifdef USE_EFFECTS_MESH
  uniform float kEffectTime;
  uniform vec3 kEffectMeshCenter;
  uniform vec4 kEffectArgs;
  uniform int kEffectType;
#endif

// @varyings
out vec2 vTexCoord;


// @code
struct VertexData {
  vec4 vertex;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  vec2 texcoord;
};
#ifdef CUSTOM_VERTEX_TRANSFORM
// $CUSTOM_VERTEX_TRANSFORM
#endif

void VertexShaderMain() {
    // there was some historical reason why the 2D shapes were
    // laid out in negative Y axis in the model space and why
    // that was changed by flipping the Y here. (can't remember
    // any more why exactly).

    // Flipping the Y flips the winding order and the related
    // question is of course, which way should the surface
    // normal point?
    //
    // It could make sense that if we think of the model space
    // so that the Z axis points towards the viewer the the
    // normal would point towards the viewer and flipping the
    // Y would flip the model and thus the normal would also
    // need to change to point to negative Z direction.
    //
    // This however breaks code that uses orthonormal projection
    // for example in the graphics test app since it uses ortho
    // where Y grows down which means that the projection flips
    // the model again and then our normal pointing to negative
    // Z direction would put us on the backside for lighting
    // calculations. This would imply that using normal pointed
    // to the positive Z would be the way to go.
    //
    // But..
    //
    // The renderer in the engine has again more complicated setup
    // where the orthographic projection matrix is setup so that
    // Y grows up and then models are rotated so ASFasdgalsjuapsgu
    //
    // so for now we're just going to point the normal towards
    // negative Z and then use a trick in the graphics test app
    // to use -1.0 for z scale which will flip the normal for us.

    VertexData vs;
    vs.vertex = vec4(aPosition.x, -aPosition.y, 0.0, 1.0);
    vs.normal = vec3(0.0, 0.0, -1.0);
    vs.tangent = vec3(1.0, 0.0, 0.0);
    vs.bitangent = vec3(0.0, -1.0, 0.0);
    vs.texcoord = aTexCoord;

    #ifdef CUSTOM_VERTEX_TRANSFORM
      CustomVertexTransform(vs);
    #else
      #if defined(USE_EFFECTS_MESH)
        MeshEffect(vs);
      #endif
    #endif

    vTexCoord = vs.texcoord;

    mat4 model_inst_matrix = GetInstanceTransform();
    mat4 model_view_matrix = kModelViewMatrix * model_inst_matrix;
    vec4 view_position = model_view_matrix * vs.vertex;

    vs_out.view_position = view_position;
    vs_out.clip_position = kProjectionMatrix * view_position;

    if (vs_out.need_tbn) {
        mat3 normal_matrix = mat3(transpose(inverse(model_view_matrix)));
        vs_out.view_normal    = normalize(normal_matrix * vs.normal);
        vs_out.view_tangent   = normalize(normal_matrix * vs.tangent);
        vs_out.view_bitangent = normalize(normal_matrix * vs.bitangent);
        vs_out.have_tbn = true;
    }
}

)CPP_RAW_STRING"
