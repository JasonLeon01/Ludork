---@meta Global.Utils.Render

---@class Global.Utils.Render.ActorVisual
---@field texture             sf.Texture
---@field texturePath         string | nil
---@field textureNativeHandle integer | nil
---@field rect                sf.IntRect | nil
---@field textureRect         sf.IntRect | nil
---@field scale               sf.Vector2f
---@field material            string
---@field materialParameters  table<string, any>
---@field animatable          boolean | nil
---@field switchInterval      number | nil

---@class Global.Utils.Render.Module
---@field GetRealSize fun(inSize: sf.Vector2i | sf.Vector2u | sf.Vector2f): sf.Vector2f
local Render = {}

---@brief Convert a size vector to real-world coordinates using the current scale.
---
--- - @param inSize Size as Vector2i, Vector2u, or Vector2f.
---
--- - @return The scaled size.
---@param inSize sf.Vector2i | sf.Vector2u | sf.Vector2f
---@return sf.Vector2f
function Render.GetRealSize(inSize) end

---@param actor Engine.Actor
---@return Global.Utils.Render.ActorVisual
function Render.CaptureActorVisual(actor) end

---@param enemyID string | integer
---@param visual  Global.Utils.Render.ActorVisual
---@return tuple<string>
function Render.GetActorVisualSignature(enemyID, visual) end

---@param effectsByLayer table<string, Global.CustomEffects.ActorPixelShatterEffect[]>
---@param drawActor      fun(target: sf.RenderTarget, actor: Engine.Actor)
function Render.PrepareActorPixelShatterEffects(effectsByLayer, drawActor) end

return Render
