---@meta Source.UI.Parts.Shared.WindowBase

---@class Source.UI.Parts.Shared.WindowBase: Source.UI.UiController
---@field _pauseMarkAtlasRect sf.IntRect
---@field _pauseMarkFrameRect sf.IntRect
---@field new                 fun(model: Source.Windows.Base.WindowBase, windowSkin: sf.Image, repeated: boolean | nil, pauseMarkAtlasRect: sf.IntRect, pauseMarkFrameRect: sf.IntRect): Source.UI.Parts.Shared.WindowBase
local WindowBaseUI = {}

---@param model              Source.Windows.Base.WindowBase
---@param windowSkin         sf.Image
---@param repeated           boolean | nil
---@param pauseMarkAtlasRect sf.IntRect
---@param pauseMarkFrameRect sf.IntRect
function WindowBaseUI:init(model, windowSkin, repeated, pauseMarkAtlasRect, pauseMarkFrameRect) end

function WindowBaseUI:bind() end

---@param parent      Source.Windows.Base.WindowBase
---@param logicalSize sf.Vector2u
function WindowBaseUI:attachTo(parent, logicalSize) end

---@return Engine.Window
function WindowBaseUI:getWindow() end

---@return Engine.Canvas
function WindowBaseUI:getContent() end

---@return Engine.Button
function WindowBaseUI:getReturnButton() end

---@return Engine.Image
function WindowBaseUI:getPauseMark() end

---@return sf.Texture
function WindowBaseUI:getPauseMarkTexture() end

return WindowBaseUI
