---@meta Global.Utils.Render

---@class Global.Utils.Render.ActorVisual
---@field texture sf.Texture
---@field texturePath string|nil
---@field textureNativeHandle integer|nil
---@field rect sf.IntRect|nil
---@field textureRect sf.IntRect|nil
---@field scale sf.Vector2f
---@field shader sf.Shader|nil
---@field shaderPath string|nil
---@field shaderError boolean|nil
---@field hasShaderError boolean|nil
---@field hue number|nil
---@field animatable boolean|nil
---@field switchInterval number|nil

---@class Global.Utils.Render.ActorPreview
---@field visual Global.Utils.Render.ActorVisual
---@field texture sf.Texture
---@field rect sf.IntRect
---@field outputRect sf.IntRect
---@field sourceRect sf.IntRect
---@field scale sf.Vector2f
---@field shader sf.Shader|nil
---@field shaderPath string
---@field shaderError boolean
---@field hue number
---@field animatable boolean
---@field switchInterval number
---@field switchTimer number
---@field shaderTime number
---@field size integer
---@field hasHue boolean
---@field _hueShader sf.Shader|nil
---@field _sourceSize sf.Vector2u
---@field _bufferRect sf.IntRect
---@field _sourceSprite sf.Sprite
---@field _postSprite sf.Sprite
---@field _finalSprite sf.Sprite
---@field _effectBuffer sf.RenderTexture|nil
---@field _hueBuffer sf.RenderTexture|nil
---@field _outputBuffer sf.RenderTexture

local Render = {}

--- @brief Convert a size vector to real-world coordinates using the current scale.
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

---@param texture        sf.Texture
---@param rect           sf.IntRect
---@param animatable     boolean
---@param switchInterval number
---@param switchTimer    number
---@param deltaTime      number
---@return sf.IntRect, number, boolean
function Render.UpdateActorPreviewFrame(texture, rect, animatable, switchInterval, switchTimer, deltaTime) end

---@param visual Global.Utils.Render.ActorVisual
---@return Global.Utils.Render.ActorPreview
function Render.CreateActorPreview(visual) end

---@param preview   Global.Utils.Render.ActorPreview
---@param deltaTime number
---@return boolean
function Render.UpdateActorPreview(preview, deltaTime) end

return Render
