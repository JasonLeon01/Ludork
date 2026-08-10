local TextLayout = require("Source.TextLayout")
local Ui = require("Source.UI.Ui")

local _TEXT_CONFIG = "UI/LeftText16"
local _NAME_WIDTH = 60
local _DESCRIPTION_X = 64
local _ROW_HEIGHT = 32

local EnemyEncyclopediaSpecialRowUI = {}

function EnemyEncyclopediaSpecialRowUI:init(model)
    local descriptionWidth = math.max(1, model.width - _DESCRIPTION_X)
    self._displayName = TextLayout.fitPlainText(tostring(model.name or ""), _NAME_WIDTH, _TEXT_CONFIG)
    local wrappedDescription = TextLayout.wrapPlainText(
        tostring(model.description or ""), descriptionWidth, _TEXT_CONFIG
    )
    self._displayDescription = wrappedDescription:find("\n", 1, true) ~= nil and wrappedDescription
        or TextLayout.fitPlainText(wrappedDescription, descriptionWidth, _TEXT_CONFIG)
    local _, newlineCount = self._displayDescription:gsub("\n", "")
    local lineCount = math.max(1, newlineCount + 1)
    self._height = math.max(_ROW_HEIGHT, lineCount * _ROW_HEIGHT)
    super(EnemyEncyclopediaSpecialRowUI, self).init(model, nil)
end

function EnemyEncyclopediaSpecialRowUI:refresh()
    self:setText("Name", self._displayName)
    self:setText("Description", self._displayDescription)
end

function EnemyEncyclopediaSpecialRowUI:prepare()
    return super(EnemyEncyclopediaSpecialRowUI, self).prepare(sf.Vector2u.new(self.model.width, self._height))
end

function EnemyEncyclopediaSpecialRowUI:getHeight()
    return self._height
end

function EnemyEncyclopediaSpecialRowUI:getNameText()
    return self:requireControl("Name")
end

function EnemyEncyclopediaSpecialRowUI:getDescriptionText()
    return self:requireControl("Description")
end

return Ui.define("Parts/WindowEnemyEncyclopedia/EnemyEncyclopediaSpecialRow", EnemyEncyclopediaSpecialRowUI)
