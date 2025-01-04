R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen

float sRGB_encode(float value)
{
   return value <= 0.0031308
       ? value * 12.92
       : pow(value, 1.0/2.4) * 1.055 - 0.055;
}
vec4 sRGB_encode(vec4 color)
{
   vec4 ret;
   ret.r = sRGB_encode(color.r);
   ret.g = sRGB_encode(color.g);
   ret.b = sRGB_encode(color.b);
   ret.a = color.a; // alpha is always linear
   return ret;
}

)CPP_RAW_STRING"