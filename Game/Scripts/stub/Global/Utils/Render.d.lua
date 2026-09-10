---@meta Global.Utils.Render

---@class Global.Utils.Render.ActorVisual
---@field texture             sf.Texture
---@field texturePath         string | nil
---@field textureNativeHandle integer | nil
---@field rect                sf.IntRect | nil
---@field textureRect         sf.IntRect | nil
---@field scale               sf.Vector2f
---@field shader              sf.Shader | nil
---@field shaderPath          string | nil
---@field shaderError         boolean | nil
---@field hasShaderError      boolean | nil
---@field hue                 number | nil
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

---@param hue number
---@return number
function Render.NormaliseActorHue(hue) end

---@param hue number
---@return boolean
function Render.IsNeutralActorHue(hue) end

---@param shader  sf.Shader
---@param texture sf.Texture
---@param rect    sf.IntRect
---@param time    number
function Render.BindActorShader(shader, texture, rect, time) end

---@param actor Engine.Actor
---@return Global.Utils.Render.ActorVisual
function Render.CaptureActorVisual(actor) end

---@param enemyID string | integer
---@param visual  Global.Utils.Render.ActorVisual
---@return tuple<string>
function Render.GetActorVisualSignature(enemyID, visual) end

return Render
