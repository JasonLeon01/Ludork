local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.PlayerAttrHUD.PlayerStateRow")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local PlayerStateRowController = {}

function PlayerStateRowController:init(model)
    super(PlayerStateRowController, self).init(model)
    self._logicalSize = sf.Vector2u.new(math.floor(self.ui.designSize.x), math.floor(self.ui.designSize.y))
    self._contentPadding = self.ui.designSize.x - self.ui.controls["StateContent"]:getSize().x
    self._iconSize = self.ui.controls["IconArea"]:getSize().x
    self._width = self._iconSize + self._contentPadding
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
    self:setProperty("Icon", "visible", true)
    self:setProperty("StateName", "visible", false)
end

function PlayerStateRowController:prepare(logicalSize)
    local root = super(PlayerStateRowController, self).prepare(logicalSize or self._logicalSize)
    local canvasWidth
    if self.model.iconTexture ~= nil then
        canvasWidth = self._iconSize
        self._width = self._iconSize + self._contentPadding
    else
        local bounds = self.ui.controls["StateName"]:getLocalBounds()
        self._width = bounds.size.x + self._contentPadding
        canvasWidth = math.max(1, math.ceil(bounds.position.x + bounds.size.x))
    end
    ---@cast canvasWidth integer
    self._logicalSize = sf.Vector2u.new(math.ceil(canvasWidth + self._contentPadding), math.floor(self.ui.designSize.y))
    self.view:reflow(self._logicalSize)
    if self.model.iconTexture ~= nil then
        local textureSize = self.model.iconTexture:getSize()
        local scale = self._iconSize / math.max(textureSize.x, textureSize.y, 1.0)
        self:setProperty("Icon", "scale", sf.Vector2f.new(scale, scale))
    end
    return root
end

function PlayerStateRowController:getWidth()
    return self._width
end

return Ui.Define(View, PlayerStateRowController)
