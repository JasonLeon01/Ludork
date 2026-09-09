---@meta Source.Windows.Base.WindowBase.Controller

---@class Source.Windows.Base.WindowBase.Controller: Source.UIBase.UiController
---@field ui                  Source.UI.Parts.Shared.WindowBase
---@field _pauseMarkAtlasRect sf.IntRect
---@field _pauseMarkFrameRect sf.IntRect
---@field new                 fun(model: Source.Windows.Base.WindowBase, windowSkin: sf.Image, repeated: boolean | nil, pauseMarkAtlasRect: sf.IntRect, pauseMarkFrameRect: sf.IntRect): Source.Windows.Base.WindowBase.Controller
local WindowBaseController = {}

---@param model              Source.Windows.Base.WindowBase
---@param windowSkin         sf.Image
---@param repeated           boolean | nil
---@param pauseMarkAtlasRect sf.IntRect
---@param pauseMarkFrameRect sf.IntRect
function WindowBaseController:init(model, windowSkin, repeated, pauseMarkAtlasRect, pauseMarkFrameRect) end

function WindowBaseController:bind() end

---@param parent      Source.Windows.Base.WindowBase
---@param logicalSize sf.Vector2u
function WindowBaseController:attachTo(parent, logicalSize) end

---@return Engine.Window
function WindowBaseController:getWindow() end

---@return Engine.Canvas
function WindowBaseController:getContent() end

---@return Engine.Button
function WindowBaseController:getReturnButton() end

---@return Engine.Image
function WindowBaseController:getPauseMark() end

---@return sf.Texture
function WindowBaseController:getPauseMarkTexture() end

return WindowBaseController
