---@meta Source.UI.Init

---@class Source.UI.Init.SceneInitUI : Source.UI.UiController
---@field new fun(model: Source.Scenes.SceneInit.SceneInit, logicalSize: sf.Vector2u): Source.UI.Init.SceneInitUI
---@field publish fun(payload: table)
local SceneInitUI = {}

---@param payload table
function SceneInitUI.publish(payload) end

---@param model       Source.Scenes.SceneInit.SceneInit
---@param logicalSize sf.Vector2u
function SceneInitUI:init(model, logicalSize) end

function SceneInitUI:bind() end

function SceneInitUI:refresh() end

function SceneInitUI:onViewUpdate(payload) end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function SceneInitUI:prepare(logicalSize) end

---@return Engine.Image
function SceneInitUI:getBackground() end

---@param value number
function SceneInitUI:setProgress(value) end

return SceneInitUI
