
// built-in uniforms

#version 300 es

// @uniforms

// bitset of material flags.
uniform uint kMaterialFlags;

// material time in seconds.
uniform float kTime;

// custom uniforms that need to match the json description
uniform vec4 kColor;

uniform sampler2D kBackground;
uniform vec2 kBackgroundRect;

uniform sampler2D kMask;

uniform sampler2D kTitle;
// @varyings

#ifdef DRAW_POINTS
  // when drawing points the gl_PointCoord must be used
  // for texture coordinates and we don't have any texture
  // coordinates coming from the vertex shader.
  #define vTexCoord gl_PointCoord
#else
  in vec2 vTexCoord;
#endif

// per particle data only exists when rendering particles
#ifdef GEOMETRY_IS_PARTICLES
  // per particle alpha value.
  in float vParticleAlpha;
  // particle random value.
  in float vParticleRandomValue;
  // normalized particle lifetime.
  in float vParticleTime;
  // Angle of particle's direction vector relative to X axis.
  in float vParticleAngle;
#else
   // we can support the editor and make the per particle data
   // dummies with macros
   #define vParticleAlpha 1.0
   #define vParticleRandomValue 0.0
   #define vParticleTime kTime
   #define vParticleAngle 0.0
#endif

// tile data only exists when rendering a tile batch
#ifdef GEOMETRY_IS_TILES
  in vec2 vTileData;
#endif

float Spiral(vec2 m) {
	float r = length(m);
	float a = atan(m.y, m.x);
	float v = sin(20.0 * (sqrt(r)-0.5*a + 0.05*kTime*3.2));
	return clamp(v, 0.0, 1.0);

}

void FragmentShaderMain() {

    float t = sin(kTime*.3) * 0.5 + 0.5;

    float t2 = sin(kTime*4.0) * 0.5 + 0.5;

    float v = Spiral(vTexCoord - vec2(0.5, 0.5));

    vec2 texture_coords = vTexCoord; // * vec2(3.0, 1.0);

    vec4 texture_sample_bg = texture(kBackground, texture_coords);
    vec4 texture_sample_fg = texture(kBackground, texture_coords);

    vec4 title = texture(kTitle, texture_coords);

    float mask = texture(kMask, texture_coords).r;
    float fg = step(0.0000001, mask);
    float bg = 1.0 - fg;

    vec3 white = vec3(1.0);
    vec3 orange = vec3(1.0, 0.647, 0.0);
    vec3 tint = mix(white, orange*.3, t);


    float x = vTexCoord.y;
    float w = 0.02;
    float pos = 0.5; //sin(kTime * 0.5) * 0.5 + 0.5;
    float z = smoothstep(pos-w, pos, x) - smoothstep(pos, pos+w, x);
    float hard_split = step(pos, x);

    vec3 foo = mix(vec3(0.3, 0.4, 0.0), vec3(0.0, 0.4, 0.0), t2);

    vec3 col = texture_sample_bg.rgb + texture_sample_bg.rgb;

    vec3 background = mix(col*0.8, col*1.3, v) * bg;

    vec3 foreground = texture_sample_fg.rgb * fg;

    vec3 color = background + foreground;

    fs_out.color.rgb = color;
    fs_out.color.a = 1.0;
    fs_out.flags = kMaterialFlags;
}
