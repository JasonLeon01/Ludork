---@meta Global.CustomEffects.ActorPixelShatterEffect

--- @brief Render-thread prepared Actor defeat effect that scatters its captured world logical pixels.
---@class Global.CustomEffects.ActorPixelShatterEffect
---@field private _sourceActor Engine.Actor | nil
---@field private _shader sf.Shader
---@field private _seed number
---@field private _elapsed number
---@field private _prepared boolean
---@field private _snapshot sf.RenderTexture | nil
---@field private _snapshotOrigin sf.Vector2f | nil
---@field private _snapshotSize sf.Vector2f | nil
---@field private _vertices sf.VertexArray | nil
---@field private _renderStates sf.RenderStates | nil
---@field new fun(actor: Engine.Actor, shader: sf.Shader, seed: number): Global.CustomEffects.ActorPixelShatterEffect
local ActorPixelShatterEffect = {}

--- @brief Construct an unprepared pixel-shatter effect that retains the source Actor until render preparation.
---
--- - @param actor Source Actor whose current rendered appearance will be captured.
--- - @param shader Shared vertex and fragment shader used for pixel scattering.
--- - @param seed Deterministic per-effect random seed.
---@param actor  Engine.Actor
---@param shader sf.Shader
---@param seed   number
function ActorPixelShatterEffect:init(actor, shader, seed) end

--- @brief Return the retained source Actor before preparation, or nil after its visual has been captured.
---@return Engine.Actor | nil
function ActorPixelShatterEffect:getSourceActor() end

--- @brief Return whether render-thread preparation has completed.
---@return boolean
function ActorPixelShatterEffect:isPrepared() end

--- @brief Capture the Actor into a one-texel-per-world-logical-pixel texture and build its static pixel mesh.
---
--- This method must be called for the first time on the render thread. After the callback returns, the effect
--- owns the captured texture and releases its source Actor reference.
---
--- - @param drawActor Callback that draws the supplied Actor into the configured render texture.
---@param drawActor fun(targetRenderTexture: sf.RenderTexture, actor: Engine.Actor)
function ActorPixelShatterEffect:prepare(drawActor) end

--- @brief Advance the effect lifetime after render preparation.
---
--- Calls made before preparation do not advance the effect.
---@param deltaTime number
function ActorPixelShatterEffect:onTick(deltaTime) end

--- @brief Return whether the fixed 0.65-second effect has completed.
---@return boolean
function ActorPixelShatterEffect:isFinished() end

--- @brief Draw the prepared effect into the target using world coordinates.
---@param target sf.RenderTarget
function ActorPixelShatterEffect:draw(target) end

return ActorPixelShatterEffect
