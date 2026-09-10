local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.PlayerAttrHUD.PlayerStateRow")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _DEFAULT_WIDTH = 128
local _ROW_HEIGHT = 24

local PlayerStateRowController = {}

function PlayerStateRowController:init(model)
    self._logicalSize = sf.Vector2u.new(_DEFAULT_WIDTH, _ROW_HEIGHT)
    self._width = model.iconSize
    super(PlayerStateRowController, self).init(model)
end

function PlayerStateRowController:refresh()
    if self.model.iconTexture == nil then
        self:setText("StateName", LOC(self.model.name))
        self:setProperty("Icon", "visible", false)
        self:setProperty("StateName", "visible", true)
        return
    end

    self:setText("StateName", "")
    self.ui.controls["Icon"]:setTexture(self.model.iconTexture, true)
    local textureSize = self.model.iconTexture:getSize()
    local scale = self.model.iconSize / math.max(textureSize.x, textureSize.y, 1.0)
    self:setProperty("Icon", "scale", sf.Vector2f.new(scale, scale))
    self:setProperty("Icon", "visible", true)
    self:setProperty("StateName", "visible", false)
end

function PlayerStateRowController:prepare(logicalSize)
    local root = super(PlayerStateRowController, self).prepare(logicalSize or self._logicalSize)
    local canvasWidth
    if self.model.iconTexture ~= nil then
        self._width = self.model.iconSize
        canvasWidth = self.model.iconSize
    else
        local bounds = self.ui.controls["StateName"]:getLocalBounds()
        self._width = bounds.size.x
        canvasWidth = math.max(1, math.ceil(bounds.position.x + bounds.size.x))
    end
    ---@cast canvasWidth integer
    self._logicalSize = sf.Vector2u.new(canvasWidth, _ROW_HEIGHT)
    self.view:reflow(self._logicalSize)
    return root
end

function PlayerStateRowController:getWidth()
    return self._width
end

return Ui.Define(View, PlayerStateRowController)
