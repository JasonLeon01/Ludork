---@meta Source.Scenes.SceneMap.RegionTitle.Controller

---@class Source.Scenes.SceneMap.RegionTitle.Controller: Source.UIBase.UiController
---@field ui            Source.UI.RegionTitle
---@field new           fun(logicalSize: sf.Vector2u): Source.Scenes.SceneMap.RegionTitle.Controller
---@field Publish       fun(payload: table)
---@field refreshEvents string[]
---@field _logicalSize  sf.Vector2u
---@field _region       string | nil
---@field _showing      boolean
local RegionTitleController = {}

---@param logicalSize sf.Vector2u
function RegionTitleController:init(logicalSize) end

function RegionTitleController:bind() end

function RegionTitleController:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function RegionTitleController:prepare(logicalSize) end

function RegionTitleController:onViewUpdate(payload) end

---@param deltaTime number
function RegionTitleController:update(deltaTime) end

---@return boolean
function RegionTitleController:getVisible() end

---@return Engine.PlainText
function RegionTitleController:getText() end

function RegionTitleController:draw() end

return RegionTitleController
