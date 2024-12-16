R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// common shader functions

// do software wrapping for texture coordinates.
// used for cases when texture sub-rects are used.
float SoftwareTextureWrap(float x, float w, int mode) {
    if (mode == TEXTURE_WRAP_CLAMP) {
        return clamp(x, 0.0, w);
    } else if (mode == TEXTURE_WRAP_REPEAT) {
        return fract(x / w) * w;
    } else if (mode == TEXTURE_WRAP_MIRROR) {
        float p = floor(x / w);
        float f = fract(x / w);
        int m = int(floor(mod(p, 2.0)));
        if (m == 0)
           return f * w;

        return w - f * w;
    }
    return x;
}

vec2 WrapTextureCoords(vec2 coords, vec2 box, ivec2 wrapping_mode) {
    float x = SoftwareTextureWrap(coords.x, box.x, wrapping_mode.x);
    float y = SoftwareTextureWrap(coords.y, box.y, wrapping_mode.y);
    return vec2(x, y);
}

vec2 RotateTextureCoords(vec2 coords, float angle) {
    coords = coords - vec2(0.5, 0.5);
    coords = mat2(cos(angle), -sin(angle),
                  sin(angle),  cos(angle)) * coords;
    coords += vec2(0.5, 0.5);
    return coords;
}


)CPP_RAW_STRING"