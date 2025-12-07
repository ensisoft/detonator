R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2025 Sami Väisänen
// basic light computation

// @types

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
    uint type;
};

struct LightTransform {
    vec4 vector0;
    vec4 vector1;
    vec4 vector2;
    vec4 vector3;
};

// @uniforms

layout (std140) uniform LightArray {
    // array of lights
    Light lights[BASIC_LIGHT_MAX_LIGHTS];
    // normally in a scene with perspective projection the camera origin
    // is in the middle of the screen and when the vertices are transformed
    // to view space computing the direction vector towards the camera is
    // taking the vector towards 0.0,0.0,0.0.
    // But this does not work with orthographic projection since those
    // scenes are typically setup so that the center of the camera does
    // not map to the center of the screen.
    vec3 camera_center;
    // number of lights in the above array
    uint light_count;

    uint lighting_flags;
};

layout (std140) uniform LightTransformArray {
    LightTransform light_transforms[BASIC_LIGHT_MAX_LIGHTS];
};

uniform highp sampler2DArray kShadowMap;

// @code

float ComputeShadow(uint light_index, vec3 surface_normal) {
    mat4 light_map_transform;
    light_map_transform[0] = light_transforms[light_index].vector0;
    light_map_transform[1] = light_transforms[light_index].vector1;
    light_map_transform[2] = light_transforms[light_index].vector2;
    light_map_transform[3] = light_transforms[light_index].vector3;

    // compute the fragment position in the current light coordinate space.
    vec4 frag_pos_light_space = light_map_transform * vec4(vertexViewPosition, 1.0);
    vec3 frag_pos_light_map = frag_pos_light_space.xyz / frag_pos_light_space.w;
    frag_pos_light_map = frag_pos_light_map * 0.5 + 0.5;

    // combat shadow map edge artifacts here. If the object that is casting
    // a shadow is clipping the shadow map border and we sample from the
    // shadow map while clamping we're just going to get shadow values.
    // so try to smooth it out a little bit
    float border = 0.05;
    float x = frag_pos_light_map.x;
    float y = frag_pos_light_map.y;
    float sx = 1.0;
    float sy = 1.0;
    if (x < border)
        sx = smoothstep(0.0, border, x);
    if (x > 1.0 - border)
        sx = 1.0 - smoothstep(1.0 - border, 1.0, x);

    if (y < border)
        sy = smoothstep(0.0, border, y);
    if (y > 1.0 - border)
        sy = 1.0 - smoothstep(1.0 - border, 1.0, y);

    float shadow_map_layer = float(light_index);

    float shadow_map_depth = texture(kShadowMap, vec3(frag_pos_light_map.xy, shadow_map_layer)).r;
    float fragment_depth = frag_pos_light_map.z;

    vec3 direction_towards_light = normalize(lights[light_index].position -vertexViewPosition);

    //float bias = max(0.05 * (1.0 * dot(direction_towards_light, surface_normal)), 0.005);
    float bias = 0.005;

    if (fragment_depth - bias > shadow_map_depth) {
        // the fragment is in shadow
        return 1.0 * sx * sy;
    }

    return 0.0;
}

vec4 ComputeBasicLight() {
   vec4 color = fs_out.color;

    vec4 material_diffuse_color = color;
    vec4 material_ambient_color = color;
    vec4 material_specular_color = color;
    float material_specular_exponent = 4.0;

    if (fs_out.have_material_colors) {
        material_ambient_color = fs_out.ambient_color;
        material_diffuse_color = fs_out.diffuse_color;
        material_specular_color = fs_out.specular_color;
        material_specular_exponent = fs_out.specular_exponent;
    }

    vec3 surface_normal = vertexViewNormal;
    if (fs_out.have_surface_normal) {
        surface_normal = TBN * fs_out.surface_normal;
    }

    // todo: think about the meaning of alpha in this context.
    // using alpha from the incoming color values since it's
    // also used with "sprite cutouts".
    vec4 total_color = vec4(0.0, 0.0, 0.0, color.a);

    for (uint i=uint(0); i<light_count; ++i) {

        Light light = lights[i];

        if (light.type == BASIC_LIGHT_TYPE_AMBIENT) {
            total_color += (light.ambient_color * material_ambient_color);
        } else {
            float light_strength = 1.0;
            float light_spot_factor = 1.0;
            vec3 direction_towards_light;
            vec3 direction_towards_point;

            float shadow_factor = 1.0;
            if ((lighting_flags & BASIC_LIGHTING_FLAGS_ENABLE_SHADOWS) == BASIC_LIGHTING_FLAGS_ENABLE_SHADOWS)
                shadow_factor = 1.0 - ComputeShadow(i, surface_normal);

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
            float light_reflectance_factor = dot(surface_normal, direction_towards_light);

            if (light_strength > 0.0 && light_reflectance_factor > 0.0) {

                vec3 direction_towards_eye = normalize(camera_center - vertexViewPosition);
                vec3 reflected_light_direction = reflect(direction_towards_point, surface_normal);
                float light_specular_factor = pow(max(dot(direction_towards_eye, reflected_light_direction), 0.0), material_specular_exponent);

                // specular component for the light against the material
                specular_color = light.specular_color * material_specular_color * light_strength * light_specular_factor * light_spot_factor;

                // diffuse component for the light against the material
                diffuse_color = light.diffuse_color * material_diffuse_color * light_strength * light_reflectance_factor * light_spot_factor;
            }

            // total light accumulation
            total_color += ambient_color;
            total_color += (shadow_factor * diffuse_color + shadow_factor * specular_color);
            //total_color += (ambient_color + diffuse_color + specular_color);

            //total_color = vec4(shadow_factor, shadow_factor, shadow_factor, 1.0);
        }
    }

    // debug the normals
    // keep in mind that when debugging normal *maps* the maps are
    // in *LINEAR* color space (at least they should be!) since they
    // don't encode color data but vectors and those vectors should
    // have equal precision regardless of whether they're collinear
    // with the negative or positive axis.
    // HOWEVER regular image viewers don't understand this and will
    // assume they're sRGB *images* and thus will decode the data
    // which will result the displayed image showing darker than
    // what it should. SO if you're comparing the output of this
    // the image produced by writing out the normal here against
    // the normal map image that is open in some image viewer you
    // can expect the colors not to match!

    //total_color = vec4(surface_normal*0.5 + 0.5, 1.0);
    return total_color;
}

)CPP_RAW_STRING"
