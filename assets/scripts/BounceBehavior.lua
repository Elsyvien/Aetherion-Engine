-- BounceBehavior
-- Example Lua script for bouncing an entity up and down

local BounceBehavior = {}

-- Configuration
BounceBehavior.height = 1.5    -- Maximum bounce height
BounceBehavior.speed = 2.0     -- Bounce speed multiplier
BounceBehavior.time = 0        -- Internal timer
BounceBehavior.baseY = nil     -- Starting Y position

function BounceBehavior:on_create()
    aetherion.log.info("BounceBehavior started on entity: " .. self.entity_name)
    
    -- Store the initial Y position
    local entity = aetherion.getEntity(self.entity_id)
    if entity then
        local transform = entity:getTransform()
        if transform then
            local pos = transform:getPosition()
            self.baseY = pos.y
        end
    end
end

function BounceBehavior:on_update(dt)
    self.time = self.time + dt
    
    local entity = aetherion.getEntity(self.entity_id)
    if entity and self.baseY then
        local transform = entity:getTransform()
        if transform then
            local pos = transform:getPosition()
            -- Calculate bounce using sine wave
            local bounce = math.abs(math.sin(self.time * self.speed * math.pi)) * self.height
            transform:setPosition(pos.x, self.baseY + bounce, pos.z)
        end
    end
end

function BounceBehavior:on_destroy()
    aetherion.log.info("BounceBehavior stopped")
end

return BounceBehavior
