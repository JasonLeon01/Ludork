local Engine = require("Engine")
local EnemyText = require("Source.EnemyText")
local Locale = require("Source.Locale.Core")
local NodeUtils = require("Source.NodeFunctions.Utils")
local Ui = require("Source.UI.Ui")
local WindowEnemyBookUI = require("Source.UI.WindowEnemyBook")
local EnemyEncyclopediaInfoPairUI = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair")
local EnemyEncyclopediaSpecialRowUI = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local TextLayout = Engine.TextLayout
local ToShortNumber = NodeUtils.ToShortNumber

local _PORTRAIT_AREA_HEIGHT = Engine.CellSize
local _NAME_TOP_MARGIN = 8
local _INFO_TOP_MARGIN = 8
local _INFO_PAIR_WIDTH = 200
local _INFO_ROW_GAP = 32
local _INFO_LAYER_HEIGHT = 96
local _DESC_TOP_MARGIN = 8
local _DESC_LINE_GAP = 22
local _DESC_MAX_LINES = 2
local _SPECIAL_TOP_MARGIN = 16

---@class Source.UI.WindowEnemyEncyclopedia
local WindowEnemyEncyclopediaUI = {}
---@type function
local formatHitCount
---@type function
local limitLines

function WindowEnemyEncyclopediaUI:init(model, size)
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self._infoPairControllers = {}
    self._specialRowControllers = {}
    self._entry = nil
    super(WindowEnemyEncyclopediaUI, self).init(model, nil)
end

function WindowEnemyEncyclopediaUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._portraitControl = self:requireControl("Portrait")
    self._nameControl = self:requireControl("Name")
    self._infoLayer = self:requireControl("InfoLayer")
    self._descriptionControl = self:requireControl("Description")
    self._specialScrollBox = self:requireControl("SpecialScrollBox")
    self._specialList = self:requireControl("SpecialList")
    ---@cast self._infoLayer Engine.ListView
    ---@cast self._specialScrollBox Engine.ScrollBox
    ---@cast self._specialList Engine.ListView
end

function WindowEnemyEncyclopediaUI:refresh()
    self:clearEnemyControls()
end

function WindowEnemyEncyclopediaUI:prepare()
    return super(WindowEnemyEncyclopediaUI, self).prepare(self._logicalSize)
end

function WindowEnemyEncyclopediaUI:attach()
    self:attachWindowView(self.model)
end

function WindowEnemyEncyclopediaUI:getWindowFrame()
    return self._windowFrame
end

function WindowEnemyEncyclopediaUI:getContent()
    return self._content
end

function WindowEnemyEncyclopediaUI:open(entry)
    self:clearEnemyControls()
    self._entry = entry
    self._portraitControl:setCharacter(
        entry.texture, entry.rect, entry.scale, entry.animatable, entry.switchInterval, entry.shaderPath or "",
        entry.hue or 0.0
    )
    self:setProperty("Portrait", "visible", true)
    self:_renderEntry(entry)
end

function WindowEnemyEncyclopediaUI:refreshLocale()
    if self._entry == nil then
        return
    end
    WindowEnemyBookUI.RefreshEntryLocale(self._entry)
    self:_clearTextControls()
    self:_renderEntry(self._entry)
end

---@param entry Source.UI.WindowEnemyBook.Entry
function WindowEnemyEncyclopediaUI:_renderEntry(entry)
    local contentWidth = math.max(1, math.floor(self._content:getSize().x))
    local fittedName = TextLayout.fitPlainText(tostring(entry.name or ""), contentWidth, self._nameControl)
    self:setText("Name", fittedName)
    self:setProperty("Name", "visible", true)
    local displayDescription = limitLines(
        TextLayout.wrapPlainText(tostring(entry.desc or ""), contentWidth, self._descriptionControl), _DESC_MAX_LINES,
        contentWidth, self._descriptionControl
    )
    self:setText("Description", displayDescription)
    self:setProperty("Description", "visible", true)
    self.view:reflow(self._logicalSize)

    local portraitHeight = self:_layoutPortrait()
    local nameY = portraitHeight + _NAME_TOP_MARGIN
    local nameWidth = TextLayout.measurePlainText(self._nameControl, fittedName)
    self._nameControl:setPosition(sf.Vector2f.new((contentWidth - nameWidth) / 2.0, nameY))
    local nameBottom = nameY + self._nameControl:getCharacterSize()
    local infoY = nameBottom + _INFO_TOP_MARGIN
    self:_layoutInfoLayer(contentWidth, infoY)
    self:buildInfo(entry)
    local descY = infoY + 3 * _INFO_ROW_GAP + _DESC_TOP_MARGIN
    self._descriptionControl:setPosition(sf.Vector2f.new(0.0, descY))
    self.model._infoTexts[#self.model._infoTexts + 1] = self._descriptionControl
    local specialY = descY + _DESC_MAX_LINES * _DESC_LINE_GAP + _SPECIAL_TOP_MARGIN
    self:buildSpecials(entry, specialY)
    self.model._portrait = self._portraitControl
    self.model._nameText = self._nameControl
end

---@return number
function WindowEnemyEncyclopediaUI:_layoutPortrait()
    return math.min(_PORTRAIT_AREA_HEIGHT, self._portraitControl:getSize().y)
end

---@param contentWidth integer
---@param infoY        number
function WindowEnemyEncyclopediaUI:_layoutInfoLayer(contentWidth, infoY)
    ---@cast self._infoLayer Engine.ListView
    local size = sf.Vector2i.new(contentWidth, _INFO_LAYER_HEIGHT)
    ---@cast size sf.Vector2i
    self._infoLayer:setSize(size)
    self._infoLayer:setPosition(sf.Vector2f.new(0.0, infoY))
end

function WindowEnemyEncyclopediaUI:buildInfo(entry)
    local rows = {
        {
            {
                LOC("HP"),
                entry.MAXHP or 0
            },
            {
                LOC("ATK"),
                entry.ATK or 0
            },
            {
                LOC("DEF"),
                entry.DEF or 0
            }
        },
        {
            {
                LOC("EXP"),
                entry.EXP or 0
            },
            {
                LOC("GOLD"),
                entry.GOLD or 0
            },
            {
                LOC("DMG"),
                entry.damage or "???"
            }
        }
    }
    for _, row in ipairs(rows) do
        for _, pair in ipairs(row) do
            self:addInfoPair(pair[1], tostring(ToShortNumber(pair[2])))
        end
    end
    local criticalText = EnemyText.FormatCritical(entry.critical)
    local hitCount = formatHitCount(entry.hitCount)
    if bool(hitCount) then
        self:addInfoPair(LOC("HIT"), hitCount)
    end
    if bool(criticalText) then
        self:addInfoPair(LOC("CRIT"), criticalText)
    end
end

function WindowEnemyEncyclopediaUI:addInfoPair(label, value)
    local controller = EnemyEncyclopediaInfoPairUI.new({
        label = label,
        value = value
    })
    local logicalSize = sf.Vector2u.new(_INFO_PAIR_WIDTH, _INFO_ROW_GAP)
    ---@cast logicalSize sf.Vector2u
    local root = controller:prepare(logicalSize)
    ---@cast self._infoLayer Engine.ListView
    self._infoLayer:addChild(root)
    self._infoPairControllers[#self._infoPairControllers + 1] = controller
    self.model._infoTexts[#self.model._infoTexts + 1] = controller:getLabel()
    self.model._infoTexts[#self.model._infoTexts + 1] = controller:getValue()
end

function WindowEnemyEncyclopediaUI:buildSpecials(entry, y)
    local contentSize = self._content:getSize()
    local contentWidth = math.max(1, math.floor(contentSize.x))
    local listHeight = math.max(1, math.floor(contentSize.y - y))
    self._specialScrollBox:setPosition(sf.Vector2f.new(0.0, y))
    self._specialScrollBox:resize(sf.Vector2f.new(contentWidth, listHeight))
    self._specialList:setPosition(sf.Vector2f.new(0.0, 0.0))
    local listSize = sf.Vector2u.new(contentWidth, listHeight)
    ---@cast listSize sf.Vector2u
    self._specialList:setSize(listSize)
    local specialDetails = entry.specialDetails or {}
    for _, special in ipairs(specialDetails) do
        local controller = EnemyEncyclopediaSpecialRowUI.new({
            width = contentWidth,
            name = tostring(special.name or ""),
            description = tostring(special.desc or "")
        })
        local root = controller:prepare()
        self._specialList:addChild(root)
        self._specialRowControllers[#self._specialRowControllers + 1] = controller
        self.model._infoTexts[#self.model._infoTexts + 1] = controller:getNameText()
        self.model._infoTexts[#self.model._infoTexts + 1] = controller:getDescriptionText()
    end
end

function formatHitCount(hitCount)
    if hitCount == nil then
        return ""
    end
    return tostring(ToShortNumber(math.max(1, hitCount)))
end

function limitLines(text, maxLines, maxWidth, control)
    if not bool(text) then
        return ""
    end
    local lines = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do
        lines[#lines + 1] = line
    end
    if #lines <= maxLines then
        return text
    end
    local limitedLines = {}
    for index = 1, maxLines do
        limitedLines[index] = lines[index]
    end
    if bool(limitedLines) then
        limitedLines[#limitedLines] = TextLayout.fitPlainText(limitedLines[#limitedLines] .. ".", maxWidth, control)
    end
    return table.concat(limitedLines, "\n")
end

function WindowEnemyEncyclopediaUI:clearEnemyControls()
    self:_clearTextControls()
    self:setProperty("Portrait", "visible", false)
    self._entry = nil
    self._portraitControl:resetAnimation()
    self.model._portrait = nil
    self.model._nameText = nil
end

function WindowEnemyEncyclopediaUI:_clearTextControls()
    ---@cast self._infoLayer Engine.ListView
    ---@cast self._specialList Engine.ListView
    for _, controller in ipairs(self._infoPairControllers) do
        controller:dispose()
    end
    for _, controller in ipairs(self._specialRowControllers) do
        controller:dispose()
    end
    self._infoLayer:clearChildren()
    self._specialList:clearChildren()
    self._infoPairControllers = {}
    self._specialRowControllers = {}
    self:setProperty("Name", "visible", false)
    self:setProperty("Description", "visible", false)
    self:setText("Name", "")
    self:setText("Description", "")
    self.model._infoTexts = {}
end

return Ui.Define("WindowEnemyEncyclopedia", WindowEnemyEncyclopediaUI)
