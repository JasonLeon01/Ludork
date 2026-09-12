local Engine = require("Engine")
local Locale = require("Source.Locale.Core")
local NumberFormat = require("Source.Utils.NumberFormat")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local ToShortNumber = NumberFormat.ToShortNumber
local TextLayout = Engine.TextLayout

local _ICON_AREA_WIDTH = 64
local _SPECIAL_ICON_SIZE = 16
local _SPECIAL_GAP = 4
local _SPECIAL_RIGHT_PAD = 4
local _SPECIAL_NAME_MAX_WIDTH = 80
local _STAT_TEXT_COLOUR = sf.Color.new(255, 255, 255, 255)
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
---@type function
local measureSpecialAreaWidth

function WindowEnemyBookCellController:init(model)
    local entry = model.entry
    self._specialDisplays = entry.specialDisplays or {}
    self._specialDisplayTexts = {}
    super(WindowEnemyBookCellController, self).init(model, nil)
end

function WindowEnemyBookCellController:bind()
    self._specialIcons = {
        self.ui.controls["SpecialIcon1"], self.ui.controls["SpecialIcon2"], self.ui.controls["SpecialIcon3"]
    }
    self._specialTexts = {
        self.ui.controls["SpecialText1"], self.ui.controls["SpecialText2"], self.ui.controls["SpecialText3"]
    }
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
    local specialAreaWidth = measureSpecialAreaWidth(self._specialDisplays, self._specialTexts)
    local nameMaxWidth = math.max(32, math.floor(assert(self._viewLogicalSize).x - _ICON_AREA_WIDTH - specialAreaWidth))
    self:setText("Name", TextLayout.fitPlainText(self.model.entry.name or "", nameMaxWidth, self.ui.controls["Name"]))
    for _, stat in ipairs(_STAT_FIELDS) do
        local value = tostring(ToShortNumber(self.model.entry[stat.field] or stat.default))
        local colour = value == "???" and _UNDEFEATABLE_TEXT_COLOUR or _STAT_TEXT_COLOUR
        self:setText(stat.labelControl, LOC(stat.locale))
        self:setText(stat.valueControl, value)
        self:setProperty(stat.labelControl, "colour", colour)
        self:setProperty(stat.valueControl, "colour", colour)
    end
    for index = 1, 3 do
        self:setProperty("SpecialIcon" .. tostring(index), "visible", false)
        self:setProperty("SpecialText" .. tostring(index), "visible", false)
        self:setText("SpecialText" .. tostring(index), "")
        self._specialDisplayTexts[index] = ""
    end
    for index = 1, math.min(#self._specialDisplays, 3) do
        local item = assert(self._specialDisplays[index])
        if item.texture ~= nil then
            local icon = assert(self._specialIcons[index])
            icon:setTexture(item.texture, true)
            self:setProperty("SpecialIcon" .. tostring(index), "visible", true)
        else
            local specialText = assert(self._specialTexts[index])
            local displayName = TextLayout.fitPlainText(tostring(item.name or ""), _SPECIAL_NAME_MAX_WIDTH, specialText)
            self._specialDisplayTexts[index] = displayName
            self:setText("SpecialText" .. tostring(index), displayName)
            self:setProperty("SpecialText" .. tostring(index), "visible", true)
        end
    end
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

function measureSpecialAreaWidth(specialDisplays, specialTexts)
    if not bool(specialDisplays) then
        return 0.0
    end
    local width = _SPECIAL_RIGHT_PAD + 0.0
    for index = 1, math.min(#specialDisplays, 3) do
        local item = assert(specialDisplays[index])
        if index > 1 then
            width = width + _SPECIAL_GAP
        end
        if item.texture ~= nil then
            width = width + _SPECIAL_ICON_SIZE
        else
            local specialText = assert(specialTexts[index])
            local displayName = TextLayout.fitPlainText(tostring(item.name or ""), _SPECIAL_NAME_MAX_WIDTH, specialText)
            width = width + TextLayout.measurePlainText(specialText, displayName)
        end
    end
    return width
end

function WindowEnemyBookCellController:_layoutSpecials()
    local currentX = assert(self._viewLogicalSize).x - _SPECIAL_RIGHT_PAD + 0.0
    for index = math.min(#self._specialDisplays, 3), 1, -1 do
        local item = assert(self._specialDisplays[index])
        if item.texture ~= nil then
            local icon = assert(self._specialIcons[index])
            local textureSize = item.texture:getSize()
            local scale = _SPECIAL_ICON_SIZE / math.max(textureSize.x, textureSize.y, 1.0)
            icon:setScale(sf.Vector2f.new(scale, scale))
            local iconX = currentX - _SPECIAL_ICON_SIZE
            icon:setPosition(sf.Vector2f.new(iconX, 0.0))
            currentX = iconX - _SPECIAL_GAP
        else
            local displayName = assert(self._specialDisplayTexts[index])
            local specialText = assert(self._specialTexts[index])
            local textWidth = TextLayout.measurePlainText(specialText, displayName)
            specialText:setPosition(sf.Vector2f.new(currentX, 0.0))
            currentX = currentX - textWidth - _SPECIAL_GAP
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
