---@meta Global.CustomParticles.DamageTextParticle
---@class DamageTextParticle
---@field _particleSystem   Engine.ParticleSystem
---@field _speedCurve       Engine.Curve
---@field _textParticle     Engine.TextParticle | nil
---@field _startPosition    sf.Vector2f
---@field _destroyRequested boolean
---@field _destroyed        boolean
local DamageTextParticle = {}

---@return DamageTextParticle
function DamageTextParticle.new(...) end

--- @brief Floating damage text particle for the map particle system.
---
--- The particle starts from the given map-view position, moves 32px right at a fixed
--- speed, animates vertical offset via a curve, and removes itself after 0.5 seconds.

--- @brief Construct and add a damage text particle to a particle system.
--- - @param particleSystem Target particle system used to render the text.
--- - @param text Text content, usually a damage number.
--- - @param position Initial map-view position.
--- - @param textConfig Plain text configuration asset used to render the number.
---@param particleSystem Engine.ParticleSystem
---@param text           string
---@param position       sf.Vector2f
---@param textConfig     Engine.PlainTextConfig
---@param speedCurve     Engine.Curve
function DamageTextParticle:init(particleSystem, text, position, textConfig, speedCurve) end

--- @brief Remove this damage text from its particle system.
function DamageTextParticle:destroy() end

---@param countTime number
function DamageTextParticle:update(countTime) end

return DamageTextParticle
