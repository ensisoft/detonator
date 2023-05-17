function IsAlive(entity)
    if entity.health > 0.0 then
        return true
    end
    return false
end

function DealDamage(entity, damage)
    local health_node = entity:FindNodeByClassName('Health')
    local health_draw = health_node:GetDrawable()

    entity.health = entity.health - damage
    if entity.health > 0.0 then
        health_draw:SetUniform('kValue', entity.health)
        return
    end

    health_draw:SetVisible(false)

    local hit_body_node = entity:FindNodeByClassName('HitBody')
    local spatial_node = hit_body_node:GetSpatialNode()
    spatial_node:Enable(false)
end

