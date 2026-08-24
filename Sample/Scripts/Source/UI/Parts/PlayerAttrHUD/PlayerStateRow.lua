local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _DEFAULT_WIDTH = 128
local _ROW_HEIGHT = 24

local PlayerStateRowUI = {}

function PlayerStateRowUI:init(model)
    self._logicalSize = sf.Vector2u.new(_DEFAULT_WIDTH, _ROW_HEIGHT)
    self._width = model.iconSize
    super(PlayerStateRowUI, self).init(model)
end

function PlayerStateRowUI:bind()
    self._icon = self:requireControl("Icon")
    self._nameText = self:requireControl("StateName")
end

function PlayerStateRowUI:refresh()
    if self.model.iconTexture == nil then
        self:setText("StateName", LOC(self.model.name))
        self:setProperty("Icon", "visible", false)
        self:setProperty("StateName", "visible", true)
        return
    end

    self:setText("StateName", "")
    self._icon:setTexture(self.model.iconTexture, true)
    local textureSize = self.model.iconTexture:getSize()
    local scale = self.model.iconSize / math.max(textureSize.x, textureSize.y, 1.0)
    self:setProperty("Icon", "scale", { scale, scale })
    self:setProperty("Icon", "visible", true)
    self:setProperty("StateName", "visible", false)
end

function PlayerStateRowUI:prepare(logicalSize)
    local root = super(PlayerStateRowUI, self).prepare(logicalSize or self._logicalSize)
    local canvasWidth
    if self.model.iconTexture ~= nil then
        self._width = self.model.iconSize
        canvasWidth = self.model.iconSize
    else
        local bounds = self._nameText:getLocalBounds()
        self._width = bounds.size.x
        canvasWidth = math.max(1, math.ceil(bounds.position.x + bounds.size.x))
    end
    ---@cast canvasWidth integer
    self._logicalSize = sf.Vector2u.new(canvasWidth, _ROW_HEIGHT)
    self.view:reflow(self._logicalSize)
    return root
end

function PlayerStateRowUI:getWidth()
    return self._width
end

return Ui.Define("Parts/PlayerAttrHUD/PlayerStateRow", PlayerStateRowUI)
