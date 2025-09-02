R"CPP_RAW_STRING(//"

void MeshExplosionEffect(inout VertexData vs) {
    float shard_linear_speed = kEffectArgs.x;
    float shard_linear_acceleration = kEffectArgs.y;
    float shard_rotational_speed = kEffectArgs.z;
    float shard_rotational_acceleration = kEffectArgs.w;

    vec3 shard_center = aEffectShardData.xyz;
    float shard_random = aEffectShardData.w;
    shard_center.y *= -1.0;

    vec3 mesh_center = kEffectMeshCenter.xyz;
    mesh_center.y *= -1.0;

    // add shard rotation (radians)
    shard_rotational_speed += (shard_rotational_acceleration * kEffectTime);
    shard_rotational_speed *= shard_random;
    float shard_angle = shard_rotational_speed * kEffectTime;

    mat2 shard_rotation = mat2(cos(shard_angle), -sin(shard_angle),
                               sin(shard_angle), cos(shard_angle));
    vec4 vertex = vs.vertex;
    vertex.xy -= shard_center.xy;
    vertex.xy = shard_rotation * vertex.xy;
    vertex.xy += shard_center.xy;

    // add displacement
     vec3 shard_direction = shard_center - mesh_center;
    shard_linear_speed += (shard_linear_acceleration * kEffectTime);
    shard_linear_speed *= (shard_random * 10.0);

    vertex.xy += (shard_direction.xy * shard_linear_speed * kEffectTime);

    vs.vertex = vertex;
}

void MeshEffect(inout VertexData vs) {
    if (kEffectType == MESH_EFFECT_TYPE_EXPLOSION) {
        MeshExplosionEffect(vs);
    }
}

)CPP_RAW_STRING"