---@meta Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview

---@class Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview: Source.UI.UiController
---@field new                    fun(model: table, size: sf.Vector2i, loadPreview: function, resolvePreviewMapPath?: function, instance?: Engine.AssetInstance): Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview
---@field model                  Source.Windows.WindowFloorMapPreview
---@field _mapKey                string | nil
---@field _telepoints            sf.Vector2u[]
---@field _currentListKey        tuple<any> | nil
---@field _currentPreviewKey     tuple<any> | nil
---@field _previewTextureCache   dict<tuple<any>, sf.Texture>
---@field _loadPreview           function
---@field _resolvePreviewMapPath function | nil
---@field _logicalSize           sf.Vector2u
---@field _listView              Engine.ListView
---@field _previewImage          Engine.Image
local WindowFloorMapPreviewUI = {}

---@param model                 Source.Windows.WindowFloorMapPreview
---@param size                  sf.Vector2i
---@param loadPreview           function
---@param resolvePreviewMapPath function | nil
function WindowFloorMapPreviewUI:init(model, size, loadPreview, resolvePreviewMapPath, instance) end

function WindowFloorMapPreviewUI:bind() end

function WindowFloorMapPreviewUI:refresh() end

---@return Engine.Canvas
function WindowFloorMapPreviewUI:prepare() end

function WindowFloorMapPreviewUI:attach(nested) end

---@return Engine.Window
function WindowFloorMapPreviewUI:getWindowFrame() end

---@return Engine.Canvas
function WindowFloorMapPreviewUI:getContent() end

---@return Engine.ListView
function WindowFloorMapPreviewUI:getListView() end

---@return Engine.ScrollBox
function WindowFloorMapPreviewUI:getScrollBox() end

function WindowFloorMapPreviewUI:clearPreviewCache() end

---@param active    boolean
---@param wasActive boolean
function WindowFloorMapPreviewUI:onActiveChanged(active, wasActive) end

---@param mapKey        string | nil
---@param entries       table
---@param selectedIndex integer
function WindowFloorMapPreviewUI:setMapKeyAndTelepoints(mapKey, entries, selectedIndex) end

---@param previousIndex integer | nil
function WindowFloorMapPreviewUI:afterSelectionUpdate(previousIndex) end

---@param entries table
function WindowFloorMapPreviewUI:rebuildTelepointList(entries) end

function WindowFloorMapPreviewUI:refreshSelectedPreview() end

---@return sf.Vector2u | nil
function WindowFloorMapPreviewUI:getSelectedTelepoint() end

function WindowFloorMapPreviewUI:hidePreview() end

---@return integer
function WindowFloorMapPreviewUI.GetTelepointItemWidth() end

return WindowFloorMapPreviewUI
