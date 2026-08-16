---@meta Source.UI.Parts.Shared.WindowBase

---@param model      Source.Windows.Base.WindowBase
---@param windowSkin sf.Image
---@param repeated   boolean | nil
function WindowBaseUI:init(model, windowSkin, repeated) end

function WindowBaseUI:bind() end

---@param parent      Engine.Canvas
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
