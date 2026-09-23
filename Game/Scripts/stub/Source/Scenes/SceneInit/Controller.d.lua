---@meta Source.Scenes.SceneInit.Controller

---@class Source.Scenes.SceneInit.Controller: Internal.UIBase.UiController
---@field ui      Internal.UI.Init
---@field new     fun(model: Source.Scenes.SceneInit.SceneInit, logicalSize: sf.Vector2u): Source.Scenes.SceneInit.Controller
---@field Publish fun(payload: table)
local SceneInitController = {}

---@param payload table
function SceneInitController.Publish(payload) end

---@param model       Source.Scenes.SceneInit.SceneInit
---@param logicalSize sf.Vector2u
function SceneInitController:init(model, logicalSize) end

function SceneInitController:bind() end

function SceneInitController:refresh() end

function SceneInitController:onViewUpdate(payload) end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function SceneInitController:prepare(logicalSize) end

---@return Engine.Image
function SceneInitController:getBackground() end

---@param value number
function SceneInitController:setProgress(value) end

return SceneInitController
