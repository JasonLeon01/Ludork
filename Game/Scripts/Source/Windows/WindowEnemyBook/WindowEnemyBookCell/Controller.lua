local Engine = require("Engine")
local Locale = require("Source.Locale.Core")
local NumberFormat = require("Source.Utils.NumberFormat")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowEnemyBook.WindowEnemyBookCell")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local ToShortNumber = NumberFormat.ToShortNumber
local TextLayout = Engine.TextLayout

local _UNDEFEATABLE_TEXT_COLOUR = sf.Color.new(255, 96, 96, 255)
local _STAT_FIELDS = {
    {
        labelControl = "HPLabel",
        valueControl = "HPValue",
        locale = "HP",
        field = "MAXHP",
        default = 0
    },
    {
        labelControl = "ATKLabel",
        valueControl = "ATKValue",
        locale = "ATK",
        field = "ATK",
        default = 0
    },
    {
        labelControl = "DEFLabel",
        valueControl = "DEFValue",
        locale = "DEF",
        field = "DEF",
        default = 0
    },
    {
        labelControl = "EXPLabel",
        valueControl = "EXPValue",
        locale = "EXP",
        field = "EXP",
        default = 0
    },
    {
        labelControl = "GOLDLabel",
        valueControl = "GOLDValue",
        locale = "GOLD",
        field = "GOLD",
        default = 0
    },
    {
        labelControl = "DamageLabel",
        valueControl = "DamageValue",
        locale = "DMG",
        field = "damage",
        default = "--"
    }
}

---@class Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller
local WindowEnemyBookCellController = {}

function WindowEnemyBookCellController:init(model)
    local entry = model.entry
    self._specialDisplays = entry.specialDisplays or {}
    self._specialDisplayTexts = {}
    super(WindowEnemyBookCellController, self).init(model, nil)
end

function WindowEnemyBookCellController:bind()
    self._statColours = {}
    for _, stat in ipairs(_STAT_FIELDS) do
        local label = self.ui.controls[stat.labelControl]
        local value = self.ui.controls[stat.valueControl]
        ---@cast label Engine.PlainText
        ---@cast value Engine.PlainText
        self._statColours[stat.labelControl] = label:getColour():copy()
        self._statColours[stat.valueControl] = value:getColour():copy()
    end
    self._specialViews = { self.ui.assets["Special1"], self.ui.assets["Special2"], self.ui.assets["Special3"] }
    self._specialPadding = {}
    self._specialNameWidths = {}
    for index, ui in ipairs(self._specialViews) do
        self._specialPadding[index] = ui.root:getSize().x - ui.controls["Content"]:getSize().x
        self._specialNameWidths[index] = ui.designSize.x - self._specialPadding[index]
    end
    self.ui.controls["EnemyIcon"]:setCharacter(
        self.model.entry.texture, self.model.entry.rect, self.model.entry.scale, self.model.entry.animatable,
        self.model.entry.switchInterval, self.model.entry.shaderPath or "", self.model.entry.hue or 0.0
    )
    if self.model.callback ~= nil then
        self.root:addConfirmCallback(function (obj, kwargs)
            self.model.callback(obj, kwargs)
        end)
    end
end

function WindowEnemyBookCellController:refresh()
    for _, stat in ipairs(_STAT_FIELDS) do
        local value = tostring(ToShortNumber(self.model.entry[stat.field] or stat.default))
        self:setText(stat.labelControl, LOC(stat.locale))
        self:setText(stat.valueControl, value)
        self:setProperty(
            stat.labelControl, "colour",
            value == "???" and _UNDEFEATABLE_TEXT_COLOUR or self._statColours[stat.labelControl]
        )
        self:setProperty(
            stat.valueControl, "colour",
            value == "???" and _UNDEFEATABLE_TEXT_COLOUR or self._statColours[stat.valueControl]
        )
    end
    local specialAreaWidth = 0.0
    for index, ui in ipairs(self._specialViews) do
        ui.controls["Icon"]:setVisible(false)
        ui.controls["Text"]:setVisible(false)
        ui.instance:setText("Text", "")
        self._specialDisplayTexts[index] = ""
        local item = self._specialDisplays[index]
        ui.root:setVisible(item ~= nil)
        if item ~= nil then
            if item.texture ~= nil then
                ui.controls["Icon"]:setTexture(item.texture, true)
                ui.controls["Icon"]:setVisible(true)
                specialAreaWidth = specialAreaWidth + ui.controls["IconArea"]:getSize().x
            else
                local text = ui.controls["Text"]
                local displayName = TextLayout.fitPlainText(
                    tostring(item.name or ""), self._specialNameWidths[index], text
                )
                self._specialDisplayTexts[index] = displayName
                ui.instance:setText("Text", displayName)
                text:setVisible(true)
                specialAreaWidth = specialAreaWidth + TextLayout.measurePlainText(text, displayName)
            end
            specialAreaWidth = specialAreaWidth + assert(self._specialPadding[index])
        end
    end
    local nameMaxWidth = math.max(1, math.floor(self.ui.controls["NameArea"]:getSize().x - specialAreaWidth))
    self:setText("Name", TextLayout.fitPlainText(self.model.entry.name or "", nameMaxWidth, self.ui.controls["Name"]))
end

function WindowEnemyBookCellController:prepare(logicalSize)
    local root = super(WindowEnemyBookCellController, self).prepare(logicalSize)
    self:_layoutSpecials()
    return root
end

function WindowEnemyBookCellController:refreshLocale()
    self:prepare()
    self.root:render()
end

function WindowEnemyBookCellController:_layoutSpecials()
    local currentX = self.root:getSize().x + 0.0
    for index = math.min(#self._specialDisplays, #self._specialViews), 1, -1 do
        local item = assert(self._specialDisplays[index])
        local ui = assert(self._specialViews[index])
        local padding = assert(self._specialPadding[index])
        local width = item.texture ~= nil and ui.controls["IconArea"]:getSize().x
            or TextLayout.measurePlainText(ui.controls["Text"], self._specialDisplayTexts[index])
        local size = sf.Vector2u.new(math.max(1, math.ceil(width + padding)), math.floor(ui.designSize.y))
        ui:prepare(size)
        local position = ui.root:getPosition()
        ui.root:setPosition(sf.Vector2f.new(currentX - size.x, position.y))
        currentX = currentX - width - padding
        if item.texture ~= nil then
            local textureSize = item.texture:getSize()
            local iconSize = ui.controls["IconArea"]:getSize()
            local scale = math.min(iconSize.x, iconSize.y) / math.max(textureSize.x, textureSize.y, 1.0)
            ui.controls["Icon"]:setScale(sf.Vector2f.new(scale, scale))
        end
    end
end

function WindowEnemyBookCellController:getIcon()
    if not self.ui.controls["EnemyIcon"]:getVisible() then
        return nil
    end
    return self.ui.controls["EnemyIcon"]
end

function WindowEnemyBookCellController:getTextureRect()
    return self.ui.controls["EnemyIcon"]:getFrameRect()
end

function WindowEnemyBookCellController:getSwitchTimer()
    return self.ui.controls["EnemyIcon"]:getSwitchTimer()
end

return Ui.Define(View, WindowEnemyBookCellController)
