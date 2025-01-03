R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// generic fragment shader main that works with the
// actual material fragment shaders

#version 300 es

// leave precision undefined here, material shader can
// define that since those can be user specified.
// precision highp float;

// this is what the material shader fills.
struct FS_OUT {
    vec4 color;

    vec4 ambient_color;
    vec4 diffuse_color;
    vec4 specular_color;
    float specular_exponent; // aka shininess
    bool use_light;
} fs_out;

// vec4 has 16 byte alignment.
// vec3 has 16 byte aligment
// vec2 has 8 byte alignment
// int, float, bool have 4 byte alignment

struct Light {
    vec4 diffuse_color;
    vec4 ambient_color;
    vec4 specular_color;
    vec3 direction; // spot/directional light direction in view space. needs to be normalized
    float spot_half_angle; // radians
    vec3 position; // light position in view space.
    float constant_attenuation;
    float linear_attenuation;
    float quadratic_attenuation;
    int type;
};

uniform int kLightCount;
// normally in a scene with perspective projection the camera origin
// is in the middle of the screen and when the vertices are transformed
// to view space computing the direction vector towards the camera is
// taking the vector towards 0.0,0.0,0.0.
// But this does not work with orthographic projection since those
// scenes are typically setup so that the center of the camera does
// not map to the center of the screen.
uniform vec3 kCameraCenter;

layout (std140) uniform LightArray {
    Light lights[BASIC_LIGHT_MAX_LIGHTS];
};


// this is the default color buffer out color.
layout (location=0) out vec4 fragOutColor;

in vec3 vertexViewPosition;
in vec3 vertexViewNormal;

void main() {
    fs_out.use_light = false;

    FragmentShaderMain();

    vec4 color = fs_out.color;

    vec4 material_diffuse_color = color;
    vec4 material_ambient_color = color;
    vec4 material_specular_color = color;
    float material_specular_exponent = 4.0;

    if (fs_out.use_light) {
        material_ambient_color = fs_out.ambient_color;
        material_diffuse_color = fs_out.diffuse_color;
        material_specular_color = fs_out.specular_color;
        material_specular_exponent = fs_out.specular_exponent;
    }

    vec4 total_color = vec4(0.0);

    for (int i=0; i<kLightCount; ++i) {

        Light light = lights[i];

        if (light.type == BASIC_LIGHT_TYPE_AMBIENT) {
            total_color += (light.ambient_color * material_ambient_color);

        } else {
            float light_strength = 1.0;
            float light_spot_factor = 1.0;
            vec3 direction_towards_light;
            vec3 direction_towards_point;

            // compute light attenuation for point and spot lights
            if (light.type == BASIC_LIGHT_TYPE_POINT || light.type == BASIC_LIGHT_TYPE_SPOT) {
                float distance_from_light = length(light.position - vertexViewPosition);

                // old OpenGL fixed function formula
                float light_attenuation = light.constant_attenuation +
                                          light.linear_attenuation * distance_from_light +
                                          light.quadratic_attenuation * pow(distance_from_light, 2.0);
                light_strength = 1.0 / light_attenuation;

                direction_towards_light = normalize(light.position - vertexViewPosition);
                direction_towards_point = -direction_towards_light;

                if (light.type == BASIC_LIGHT_TYPE_SPOT) {
                    float light_ray_angle = acos(dot(direction_towards_point, light.direction));
                    light_spot_factor = 1.0 - smoothstep(light.spot_half_angle-0.02,
                                                         light.spot_half_angle, light_ray_angle);
                }

            } else if (light.type == BASIC_LIGHT_TYPE_DIRECTIONAL) {
                direction_towards_point = light.direction;
                direction_towards_light = -direction_towards_point;
            }

            // ambient component for the light against the material
            vec4 ambient_color = light.ambient_color * material_ambient_color;
            vec4 diffuse_color = vec4(0.0);
            vec4 specular_color = vec4(0.0);

            // ideal matte reflectance value, i.e. so called Lambertinian reflectance.
            // computed by taking the angle between the surface normal the vector that
            // points to the light from that point on the surface.
            // if the light is behind the surface the dot product is negative
            // https://en.wikipedia.org/wiki/Lambertian_reflectance
            float light_reflectance_factor = dot(vertexViewNormal, direction_towards_light);

            if (light_strength > 0.0 && light_reflectance_factor > 0.0) {

                vec3 direction_towards_eye = normalize(kCameraCenter - vertexViewPosition);
                vec3 reflected_light_direction = reflect(direction_towards_point, vertexViewNormal);
                float light_specular_factor = pow(max(dot(direction_towards_eye, reflected_light_direction), 0.0), material_specular_exponent);

                // specular component for the light against the material
                specular_color = light.specular_color * material_specular_color * light_strength * light_specular_factor * light_spot_factor;

                // diffuse component for the light against the material
                diffuse_color = light.diffuse_color * material_diffuse_color * light_strength * light_reflectance_factor * light_spot_factor;
            }

            // total light accumulation
            total_color += (ambient_color + diffuse_color + specular_color);

            // debug the normals
            //total_color = vec4(vertexViewNormal, 1.0);
        }
    }
    fragOutColor = sRGB_encode(total_color);
}

)CPP_RAW_STRING"
