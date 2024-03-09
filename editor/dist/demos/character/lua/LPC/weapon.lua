function ShootArrow(entity)
    local node = entity:FindNodeByClassName('Weapon')
    local pos = Scene:MapPointFromEntityNode(entity, node,
                                             glm.vec2:new(0.0, 0.0))
    pos = pos + entity.direction * 50.0

    local arrow = Scene:SpawnEntity('LPC/Prop/Arrow', {
        pos = pos,
        logging = false,
        layer = 10
    })
    arrow.direction = entity.direction * 800.0
end

function ThrustSpear(entity)
    local node = entity:GetNode(0)
    local pos = node:GetTranslation()

    -- for the spear thrust we do a spatial query in the direction 
    -- we're currently facing for some maximum distance. The starting
    -- point is elevated a little bit because the root node (i.e. node
    -- at index 0) is at the feet of the character.
    local nodes = Scene:QuerySpatialNodes(pos + glm.vec2:new(0.0, -1.0) * 60.0,
                                          pos + entity.direction * 60.0, 'All')

    -- keep in mind that if we query for 'All' then we might found
    -- our own spatial node as well!
    while nodes:HasNext() do
        local other_node = nodes:GetNext()
        local other_entity = other_node:GetEntity()
        if other_entity ~= entity then
            -- if something was hit, call a method about being thrust by  spear
            CallMethod(other_entity, 'SpearThrust')
        end
    end
end

function SlashDagger(entity)
    local node = entity:GetNode(0)
    local pos = node:GetTranslation()

    -- for the spear thrust we do a spatial query in the direction 
    -- we're currently facing for some maximum distance. The starting
    -- point is elevated a little bit because the root node (i.e. node
    -- at index 0) is at the feet of the character.
    local nodes = Scene:QuerySpatialNodes(pos + glm.vec2:new(0.0, -1.0) * 60.0,
                                          pos + entity.direction * 60.0, 'All')

    -- keep in mind that if we query for 'All' then we might found
    -- our own spatial node as well!
    while nodes:HasNext() do
        local other_node = nodes:GetNext()
        local other_entity = other_node:GetEntity()
        if other_entity ~= entity then
            -- if something was hit, call a method about being thrust by  spear
            CallMethod(other_entity, 'DaggerStab')
        end
    end
end

function StrikeWithWeapon(entity, weapon)
    if weapon == 'Bow+Arrow' then
        ShootArrow(entity)
    elseif weapon == 'Spear' then
        ThrustSpear(entity)
    elseif weapon == 'Dagger' then
        SlashDagger(entity)
    end
end
