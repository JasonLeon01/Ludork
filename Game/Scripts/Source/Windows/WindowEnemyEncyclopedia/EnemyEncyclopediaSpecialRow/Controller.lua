local Engine = require("Engine")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow")

local TextLayout = Engine.TextLayout

local _NAME_WIDTH = 80
local _DESCRIPTION_X = 88
local _ROW_HEIGHT = 32

local EnemyEncyclopediaSpecialRowController = {}

function EnemyEncyclopediaSpecialRowController:init(model)
    self._displayName = ""
    self._displayDescription = ""
    self._height = _ROW_HEIGHT
    super(EnemyEncyclopediaSpecialRowController, self).init(model, nil)
end

function EnemyEncyclopediaSpecialRowController:refresh()
    local descriptionWidth = math.max(1, self.model.width - _DESCRIPTION_X)
    local nameControl = self.ui.controls["Name"]
    local descriptionControl = self.ui.controls["Description"]
    ---@cast nameControl Engine.PlainText
    ---@cast descriptionControl Engine.PlainText
    self._displayName = TextLayout.fitPlainText(tostring(self.model.name or ""), _NAME_WIDTH, nameControl)
    local wrappedDescription = TextLayout.wrapPlainText(
        tostring(self.model.description or ""), descriptionWidth, descriptionControl
    )
    self._displayDescription = wrappedDescription:find("\n", 1, true) ~= nil and wrappedDescription
        or TextLayout.fitPlainText(wrappedDescription, descriptionWidth, descriptionControl)
    local _, newlineCount = self._displayDescription:gsub("\n", "")
    local lineCount = math.max(1, newlineCount + 1)
    self._height = math.max(_ROW_HEIGHT, lineCount * _ROW_HEIGHT)
    self:setText("Name", self._displayName)
    self:setText("Description", self._displayDescription)
end

function EnemyEncyclopediaSpecialRowController:prepare()
    local root = super
        (EnemyEncyclopediaSpecialRowController, self)
        .prepare(sf.Vector2u.new(self.model.width, self._height))
    self.view:reflow(sf.Vector2u.new(self.model.width, self._height))
    return root
end

function EnemyEncyclopediaSpecialRowController:getHeight()
    return self._height
end

return Ui.Define(View, EnemyEncyclopediaSpecialRowController)
