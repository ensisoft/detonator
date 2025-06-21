R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// Perceptual 3D shader that is used to render 2D content
// that is perceptually 3D, such as isometric tiles.

#version 300 es

// @attributes
in vec2 aPosition;
in vec2 aTexCoord;
in vec3 aPerceptualPosition;
in vec3 aPerceptualNormal;

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
uniform vec3 kWorldPosition;
uniform vec3 kWorldSize;

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
  vec4 perceptual_vertex;
  vec3 perceptual_normal;
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
    vs.perceptual_vertex = vec4(aPerceptualPosition, 1.0);
    vs.perceptual_normal = vec4(aPerceptualNormal, 1.0);
    vs.texcoord = aTexCoord;

    #ifdef CUSTOM_VERTEX_TRANSFORM
        CustomVertexTransform(vs);
    #endif

    vTexCoord = vs.texcoord;

    // todo: instance matrix
    //mat4 model_inst_matrix = ...
    mat4 model_view_matrix = kModelViewMatrix; // * model_inst_matrix
    vec4 render_view_position = model_view_matrix * vs.render_vertex;

    // transform the perceptual position from a local coordinate to
    // the world coordinate.
    vec3 world_view_position = kWorldPosition + aPerceptualPosition * kWorldSize;
    vec3 world_view_normal = aPerceptualNormal;

    vs_out.clip_position = kProjectionMatrix * render_view_position;
    vs_out.view_position = vec4(world_view_position, 1.0);
    vs_out.view_normal   = world_view_normal;
    // this is a lie, we don't have tangent/bitangent
    // todo: maybe separate these?
    vs_out.have_tbn = true;
}

)CPP_RAW_STRING"