-- RotationBehavior
-- Example Lua script for rotating an entity

local RotationBehavior = {}

-- Configuration
RotationBehavior.speed = 45.0  -- degrees per second
RotationBehavior.axis = "y"    -- rotation axis: x, y, or z

function RotationBehavior:on_create()
    aetherion.log.info("RotationBehavior started on entity: " .. self.entity_name)
end

function RotationBehavior:on_update(dt)
    local entity = aetherion.getEntity(self.entity_id)
    if entity then
        local transform = entity:getTransform()
        if transform then
            local rot = transform:getRotation()
            local delta = self.speed * dt
            
            if self.axis == "x" then
                transform:setRotation(rot.x + delta, rot.y, rot.z)
            elseif self.axis == "z" then
                transform:setRotation(rot.x, rot.y, rot.z + delta)
            else
                -- Default to Y axis
                transform:setRotation(rot.x, rot.y + delta, rot.z)
            end
        end
    end
end

function RotationBehavior:on_destroy()
    aetherion.log.info("RotationBehavior stopped")
end

return RotationBehavior
