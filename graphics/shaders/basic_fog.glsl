R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2025 Sami Väisänen
// basic light computation

// @preprocessor
#define E 2.71828182

// @types

struct Fog {
    vec4 color;
    vec3 center;
    float density;
    float start_dist;
    float end_dist;
    uint mode;
};

// @uniforms

layout(std140) uniform FogData {
    Fog fog;
};

// @code

vec4 ComputeBasicFog(vec4 color) {
    // view space distance between position and center of fog
    float distance_to_fog = length(fog.center - vertexViewPosition);

    // normalized scaler value based on distance into fog
    float fog_scaler = 0.0;
    fog_scaler = max(distance_to_fog - fog.start_dist, 0.0) / fog.end_dist;
    fog_scaler = min(fog_scaler, 1.0);

    // final fog factor for color blend
    float fog_factor = 0.0;

    if (fog.mode == BASIC_FOG_MODE_LINEAR) {
        fog_factor = 1.0 - fog_scaler;
    } else if (fog.mode == BASIC_FOG_MODE_EXP1) {
        float fog_exponent = fog.density * fog_scaler;
        fog_factor = pow(E, -fog_exponent);
        fog_factor = min(fog_factor, 1.0);
    } else if (fog.mode == BASIC_FOG_MODE_EXP2) {
        float fog_exponent = fog.density * fog_scaler;
        fog_factor = pow(E, -fog_exponent*fog_exponent);
        fog_factor = min(fog_factor, 1.0);
    }
    // this seems odd, but if you check the curve on the Eulers
    // and rembember that negative exponent on x^-n equals 1.0/x^n
    // so we start at 1.0 and then drop as the distance increases
    return fog_factor * color + (1.0 - fog_factor) * fog.color;
}

)CPP_RAW_STRING"
