---@meta Source.UI.Parts.Shared.ActorPreviewController

---@class Source.UI.Parts.Shared.ActorPreviewController.State
---@field preview Global.Utils.Render.ActorPreview | nil
---@field texture sf.Texture | nil
---@field rect sf.IntRect | nil
---@field scale sf.Vector2f
---@field displayTexture sf.Texture | nil
---@field displayRect sf.IntRect | nil
---@field animatable boolean
---@field switchInterval number
---@field switchTimer number
---@field visible boolean

---@class Source.UI.Parts.Shared.ActorPreviewController
---@field _imageControl Engine.FunctionalImage
---@field _entry Source.UI.WindowEnemyBook.Entry | nil
---@field _preview Global.Utils.Render.ActorPreview | nil
---@field _texture sf.Texture | nil
---@field _rect sf.IntRect | nil
---@field _scale sf.Vector2f
---@field _displayTexture sf.Texture | nil
---@field _displayRect sf.IntRect | nil
---@field _animatable boolean
---@field _switchInterval number
---@field _switchTimer number
---@field _visible boolean
local ActorPreviewController = {}

---@return Source.UI.Parts.Shared.ActorPreviewController
function ActorPreviewController.new(...) end

---@param imageControl Engine.FunctionalImage
function ActorPreviewController:init(imageControl) end

---@param entry Source.UI.WindowEnemyBook.Entry
function ActorPreviewController:setEntry(entry) end

function ActorPreviewController:clear() end

---@param bounds            sf.FloatRect
---@param verticalAlignment "top" | "center"
---@return sf.Vector2f
function ActorPreviewController:layout(bounds, verticalAlignment) end

---@param deltaTime number
function ActorPreviewController:tick(deltaTime) end

---@return Source.UI.Parts.Shared.ActorPreviewController.State
function ActorPreviewController:getState() end

return ActorPreviewController
