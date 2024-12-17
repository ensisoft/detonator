R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// bitmap alpha mask text shader

#version 300 es

uniform vec4 kColor;
uniform sampler2D kTexture;

in vec2 vTexCoord;

void FragmentShaderMain() {
   float alpha = texture(kTexture, vTexCoord).a;
   vec4 color = vec4(kColor.r, kColor.g, kColor.b, kColor.a * alpha);
   fs_out.color = color;
}


)CPP_RAW_STRING"