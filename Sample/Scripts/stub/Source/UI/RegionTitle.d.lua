---@meta Source.UI.RegionTitle

---@class Source.UI.RegionTitle: Source.UI.UiController
---@field new           fun(logicalSize: sf.Vector2u): Source.UI.RegionTitle
---@field Publish       fun(payload: table)
---@field refreshEvents string[]
---@field _logicalSize  sf.Vector2u
---@field _region       string | nil
---@field _showing      boolean
---@field _text         Engine.PlainText
local RegionTitleUI = {}

---@param logicalSize sf.Vector2u
function RegionTitleUI:init(logicalSize) end

function RegionTitleUI:bind() end

function RegionTitleUI:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function RegionTitleUI:prepare(logicalSize) end

function RegionTitleUI:onViewUpdate(payload) end

---@param deltaTime number
function RegionTitleUI:update(deltaTime) end

---@return boolean
function RegionTitleUI:getVisible() end

---@return Engine.PlainText
function RegionTitleUI:getText() end

function RegionTitleUI:draw() end

return RegionTitleUI
