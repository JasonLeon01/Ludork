local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local WindowEnemyBookUI = require("Source.UI.WindowEnemyBook")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local ManagerFunctions = GlobalFunctions.Manager

local _WINDOW_SIZE = 352
local _CELL_WIDTH = 320
local _CELL_HEIGHT = 64

---@class Source.Windows.WindowEnemyBook: Source.Windows.Base.WindowSelectable
local WindowEnemyBook = {}

WindowEnemyBook.uiClass = WindowEnemyBookUI

function WindowEnemyBook:init(rect, player, onClose, onConfirm)
    super(WindowEnemyBook, self).init(rect, nil, _CELL_WIDTH, _CELL_HEIGHT, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._player = player
    self._onCloseCallback = onClose
    self._onConfirmCallback = onConfirm
    self._enemies = {}
    self._ui = self.uiClass.new(self, rect.size)
    self._ui:attach()
    self._listView = self._ui:getListView()
    self:setActive(false)
    self:setVisible(false)
end

function WindowEnemyBook:setPlayer(player)
    self._player = player
end

function WindowEnemyBook:open(gameMap)
    self:_refreshEnemies(gameMap)
    self:setVisible(true)
    self:setActive(true)
    self:requestKeyboardFocus()
end

function WindowEnemyBook:close()
    self:setVisible(false)
    self:setActive(false)
end

function WindowEnemyBook:refreshLocale()
    self._ui:refreshLocale()
end

function WindowEnemyBook:onTick(deltaTime)
    self._ui:tick(deltaTime)
    super(WindowEnemyBook, self).onTick(deltaTime)
end

---@param gameMap GameMap | nil
function WindowEnemyBook:_refreshEnemies(gameMap)
    self._ui:refreshEnemies(gameMap)
end

---@param enemy Source.Enemy
---@return table
function WindowEnemyBook:_buildEntry(enemy)
    return self._ui:buildEntry(enemy)
end

---@param special table
---@return table
function WindowEnemyBook:_buildSpecialDisplays(special)
    return self._ui:buildSpecialDisplays(special)
end

---@param special table
---@return table
function WindowEnemyBook:_buildSpecialDetails(special)
    return self._ui:buildSpecialDetails(special)
end

---@param name string
---@return string
function WindowEnemyBook:_formatName(name)
    return self._ui:formatName(name)
end

---@param text string | nil
---@return string
function WindowEnemyBook:_formatText(text)
    return self._ui:formatText(text)
end

---@param index integer
---@return sf.Vector2f
---@diagnostic disable-next-line: unused
function WindowEnemyBook:_getRectPositionForIndex(index)
    return sf.Vector2f.new(0.0, index * _CELL_HEIGHT)
end

---@return integer
---@diagnostic disable-next-line: unused
function WindowEnemyBook:_getRectWidth()
    return _CELL_WIDTH
end

function WindowEnemyBook:onReturn()
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:close()
    if self._onCloseCallback ~= nil then
        self._onCloseCallback()
    end
end

---@param entry table
function WindowEnemyBook:_confirmEnemy(entry)
    ManagerFunctions.playSE(GameSystem.GetDecisionSE())
    self:close()
    if self._onConfirmCallback ~= nil then
        self._onConfirmCallback(entry)
    end
end

WindowEnemyBook._WINDOW_SIZE = _WINDOW_SIZE

local FinalWindowEnemyBook = class(WindowEnemyBook, WindowSelectable)

return FinalWindowEnemyBook
