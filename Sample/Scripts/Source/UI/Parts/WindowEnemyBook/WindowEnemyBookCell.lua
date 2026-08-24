local Locale = require("Source.Locale.Core")
local NodeUtils = require("Source.NodeFunctions.Utils")
local TextLayout = require("Source.TextLayout")
local Ui = require("Source.UI.Ui")
local ActorPreviewController = require("Source.UI.Parts.Shared.ActorPreviewController")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local ToShortNumber = NodeUtils.ToShortNumber

local _CELL_WIDTH = 320
local _CELL_HEIGHT = 64
local _ICON_AREA_WIDTH = 64
local _SPECIAL_ICON_SIZE = 16
local _SPECIAL_GAP = 4
local _SPECIAL_RIGHT_PAD = 4
local _SPECIAL_NAME_MAX_WIDTH = 80
local _NAME_TEXT_CONFIG = "UI/Text16"
local _SPECIAL_TEXT_CONFIG = "UI/RightText10"
local _STAT_TEXT_COLOUR = { 255, 255, 255, 255 }
local _UNDEFEATABLE_TEXT_COLOUR = { 255, 96, 96, 255 }
local _STAT_FIELDS = {
    {
        control = "HPText",
        locale = "HP",
        field = "MAXHP",
        default = 0
    },
    {
        control = "ATKText",
        locale = "ATK",
        field = "ATK",
        default = 0
    },
    {
        control = "DEFText",
        locale = "DEF",
        field = "DEF",
        default = 0
    },
    {
        control = "EXPText",
        locale = "EXP",
        field = "EXP",
        default = 0
    },
    {
        control = "GOLDText",
        locale = "GOLD",
        field = "GOLD",
        default = 0
    },
    {
        control = "DamageText",
        locale = "DMG",
        field = "damage",
        default = "--"
    }
}

---@class Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell
local WindowEnemyBookCellUI = {}
---@type function
local measureSpecialAreaWidth

function WindowEnemyBookCellUI:init(model)
    local entry = model.entry
    self._specialDisplays = entry.specialDisplays or {}
    self._specialDisplayTexts = {}
    super(WindowEnemyBookCellUI, self).init(model, nil)
end

function WindowEnemyBookCellUI:bind()
    self._icon = self:requireControl("EnemyIcon")
    self._previewController = ActorPreviewController.new(self._icon)
    self._previewController:setEntry(self.model.entry)
    self._nameText = self:requireControl("Name")
    self._specialIcons = {
        self:requireControl("SpecialIcon1"), self:requireControl("SpecialIcon2"), self:requireControl("SpecialIcon3")
    }
    self._specialTexts = {
        self:requireControl("SpecialText1"), self:requireControl("SpecialText2"), self:requireControl("SpecialText3")
    }
    if self.model.callback ~= nil then
        self.root:addConfirmCallback(function (obj, kwargs)
            self.model.callback(obj, kwargs)
        end)
    end
end

function WindowEnemyBookCellUI:refresh()
    local specialAreaWidth = measureSpecialAreaWidth(self._specialDisplays)
    local nameMaxWidth = math.max(32, math.floor(_CELL_WIDTH - _ICON_AREA_WIDTH - specialAreaWidth))
    self:setText("Name", TextLayout.FitPlainText(self.model.entry.name or "", nameMaxWidth, _NAME_TEXT_CONFIG))
    for _, stat in ipairs(_STAT_FIELDS) do
        local value = LOC(stat.locale) .. "\t" .. tostring(ToShortNumber(self.model.entry[stat.field] or stat.default))
        self:setText(stat.control, value)
        self:setProperty(
            stat.control, "colour", value:sub(-3) == "???" and _UNDEFEATABLE_TEXT_COLOUR or _STAT_TEXT_COLOUR
        )
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
            local displayName = TextLayout.FitPlainText(
                tostring(item.name or ""), _SPECIAL_NAME_MAX_WIDTH, _SPECIAL_TEXT_CONFIG
            )
            self._specialDisplayTexts[index] = displayName
            self:setText("SpecialText" .. tostring(index), displayName)
            self:setProperty("SpecialText" .. tostring(index), "visible", true)
        end
    end
end

function WindowEnemyBookCellUI:prepare(logicalSize)
    local root = super(WindowEnemyBookCellUI, self).prepare(logicalSize)
    self:_layoutIcon()
    self:_layoutSpecials()
    return root
end

function WindowEnemyBookCellUI:refreshLocale()
    local logicalSize = sf.Vector2u.new(_CELL_WIDTH, _CELL_HEIGHT)
    ---@cast logicalSize sf.Vector2u
    self:prepare(logicalSize)
    self.root:render()
end

function measureSpecialAreaWidth(specialDisplays)
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
            local displayName = TextLayout.FitPlainText(
                tostring(item.name or ""), _SPECIAL_NAME_MAX_WIDTH, _SPECIAL_TEXT_CONFIG
            )
            width = width + TextLayout.MeasurePlainText(_SPECIAL_TEXT_CONFIG, displayName)
        end
    end
    return width
end

function WindowEnemyBookCellUI:_layoutIcon()
    local bounds = sf.FloatRect.new(sf.Vector2f.new(0.0, 0.0), sf.Vector2f.new(_ICON_AREA_WIDTH, _CELL_HEIGHT))
    self._previewController:layout(bounds, "center")
end

function WindowEnemyBookCellUI:_layoutSpecials()
    local currentX = _CELL_WIDTH - _SPECIAL_RIGHT_PAD + 0.0
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
            local textWidth = TextLayout.MeasurePlainText(_SPECIAL_TEXT_CONFIG, displayName)
            local specialText = assert(self._specialTexts[index])
            specialText:setPosition(sf.Vector2f.new(currentX, 0.0))
            currentX = currentX - textWidth - _SPECIAL_GAP
        end
    end
end

function WindowEnemyBookCellUI:tick(deltaTime)
    self._previewController:tick(deltaTime)
    self.root:render()
end

function WindowEnemyBookCellUI:getIcon()
    if not self._previewController:getState().visible then
        return nil
    end
    return self._icon
end

function WindowEnemyBookCellUI:getTextureRect()
    return self._previewController:getState().rect
end

function WindowEnemyBookCellUI:getSwitchTimer()
    return self._previewController:getState().switchTimer
end

return Ui.Define("Parts/WindowEnemyBook/WindowEnemyBookCell", WindowEnemyBookCellUI)
