local Engine = require("Engine")
local WindowFloorTeleporterUI = require("Source.UI.WindowFloorTeleporter")
local UiLayout = require("Source.UI.UiLayout")
local WindowFloorMapCommand = require("Source.Windows.WindowFloorTeleporter.Command")
local WindowFloorMapPreview = require("Source.Windows.WindowFloorTeleporter.Preview")
local WindowFloorTeleporterController = require("Source.Windows.WindowFloorTeleporter.Controller")

local Canvas = Engine.Canvas

local _LIST_WIDTH = 176
local _TELEPOINT_PREVIEW_WIDTH = 416
local _PREVIEW_WINDOW_HEIGHT = 240

local function getDefaultRects()
    local bounds = UiLayout.GetCenteredRect(_TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT)
    return Engine.ToIntRect(bounds.position.x, bounds.position.y, _LIST_WIDTH, _PREVIEW_WINDOW_HEIGHT),
        Engine.ToIntRect(bounds.position.x, bounds.position.y, _TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT)
end

---@class Source.Windows.WindowFloorTeleporter
local WindowFloorTeleporter = {}

WindowFloorTeleporter.controllerClass = WindowFloorTeleporterController

function WindowFloorTeleporter:init(
    inst, _listRect, previewRect, loadPreview, onConfirm, onClose, resolvePreviewMapPath, clearPreviewCache
)
    super(WindowFloorTeleporter, self).init(Engine.ToIntRect(
        previewRect.position.x, previewRect.position.y, _TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT
    ))
    self._inst = inst
    self._onConfirmCallback = onConfirm
    self._onCloseCallback = onClose
    self._clearPreviewCacheCallback = clearPreviewCache
    self._ui = WindowFloorTeleporterUI.new(self)
    self._ui:attach()
    self._transition = self._ui:createTransition(self)
    self._commandWindow = WindowFloorMapCommand.new(
        Engine.ToIntRect(0, 0, _LIST_WIDTH, _PREVIEW_WINDOW_HEIGHT), self, self._ui:getCommandAsset()
    )
    self._previewWindow = WindowFloorMapPreview.new(
        Engine.ToIntRect(0, 0, _TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT), self, loadPreview,
        resolvePreviewMapPath, self._ui:getPreviewAsset()
    )
    self:addChild(self._previewWindow)
    self:addChild(self._commandWindow)
    self._lastMapKey = nil
    self._telepointIndexes = {}
    self._telepointEntriesCache = dict()
    self._teleporterController = self.controllerClass.new(self)
    self._teleporterController:hideImmediate()
end

function WindowFloorTeleporter:getCommandWindow()
    return self._commandWindow
end

function WindowFloorTeleporter:getPreviewWindow()
    return self._previewWindow
end

function WindowFloorTeleporter:getVisible()
    return self._transition:isBlocking()
end

function WindowFloorTeleporter:open(inst)
    self._teleporterController:open(inst)
end

function WindowFloorTeleporter:close(onHidden)
    self._teleporterController:close(onHidden)
end

function WindowFloorTeleporter:closeByCancel()
    self._teleporterController:closeByCancel()
end

function WindowFloorTeleporter:refreshLocale()
    self._teleporterController:refreshLocale()
end

function WindowFloorTeleporter:activateTelepointSelector()
    self._teleporterController
        :activateTelepointSelector()
end

function WindowFloorTeleporter:activateMapList(playCancelSE)
    self._teleporterController:activateMapList(playCancelSE)
end

function WindowFloorTeleporter:confirmSelectedTelepoint()
    self._teleporterController
        :confirmSelectedTelepoint()
end

function WindowFloorTeleporter:notifyTelepointIndexMaybeChanged(index)
    self._teleporterController
        :notifyTelepointIndexMaybeChanged(index)
end

function WindowFloorTeleporter:getCurrentTelepoint()
    return self._teleporterController
        :getCurrentTelepoint()
end

function WindowFloorTeleporter:notifyMapIndexMaybeChanged(index)
    self._teleporterController
        :notifyMapIndexMaybeChanged(index)
end

function WindowFloorTeleporter.GetDefaultFloorTeleporterRects()
    return getDefaultRects()
end

function WindowFloorTeleporter:dispose()
    self._teleporterController:hideImmediate()
    self._ui:dispose()
    self._inst = nil
    self._onConfirmCallback = nil
    self._onCloseCallback = nil
    self._clearPreviewCacheCallback = nil
end

return class(WindowFloorTeleporter, Canvas)
