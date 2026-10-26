---@meta Source.Scenes.SceneMap.Windows

---@class Source.Scenes.SceneMap.Windows.Movement
---@field player  Source.MapActors.Player.Player
---@field enabled boolean

---@class Source.Scenes.SceneMap.Windows.Session: Source.Scenes.SceneMap.Windows.Movement
---@field operation Engine.AsyncOperation

---@alias Source.Scenes.SceneMap.Windows.AsyncWindow Source.Windows.WindowShop | Source.Windows.WindowAttrShop | Source.Windows.WindowPlayerName
---@alias Source.Scenes.SceneMap.Windows.BlockingWindow Source.Windows.WindowShop | Source.Windows.WindowAttrShop | Source.Windows.WindowEnemyBook | Source.Windows.WindowEnemyEncyclopedia | Source.Windows.WindowFloorTeleporter | Source.Windows.WindowPlayerName | Source.Windows.WindowItem | Source.Windows.WindowEquip | Source.Windows.WindowSaveLoad | Source.Windows.ConfigWindow | Source.Windows.WindowShopRecords

--- Owns optional map windows and their active sessions without caching the current game instance or player.
---@class Source.Scenes.SceneMap.Windows
---@field new                              fun(scene: Source.Scenes.SceneMap.SceneMap): Source.Scenes.SceneMap.Windows
---@field _scene                           Source.Scenes.SceneMap.SceneMap
---@field _windowPlayerName                Internal.UIBase.LazyWindow<Source.Windows.WindowPlayerName>
---@field _windowItem                      Internal.UIBase.LazyWindow<Source.Windows.WindowItem>
---@field _windowEquip                     Internal.UIBase.LazyWindow<Source.Windows.WindowEquip>
---@field _windowShop                      Internal.UIBase.LazyWindow<Source.Windows.WindowShop>
---@field _windowAttrShop                  Internal.UIBase.LazyWindow<Source.Windows.WindowAttrShop>
---@field _windowEnemyBook                 Internal.UIBase.LazyWindow<Source.Windows.WindowEnemyBook>
---@field _windowEnemyEncyclopedia         Internal.UIBase.LazyWindow<Source.Windows.WindowEnemyEncyclopedia>
---@field _windowFloorTeleporter           Internal.UIBase.LazyWindow<Source.Windows.WindowFloorTeleporter>
---@field _windowShopRecords               Internal.UIBase.LazyWindow<Source.Windows.WindowShopRecords>
---@field _windowSaveLoad                  Internal.UIBase.LazyWindow<Source.Windows.WindowSaveLoad>
---@field _windowMenu                      Internal.UIBase.LazyWindow<Source.Windows.WindowMenu>
---@field _configWindow                    Internal.UIBase.LazyWindow<Source.Windows.ConfigWindow>
---@field _blockingWindows                 Internal.UIBase.LazyWindow<Source.Scenes.SceneMap.Windows.BlockingWindow>[]
---@field _pendingMenuOpen                 boolean
---@field _pendingSaveLoadOpen             Enums.SaveLoadMode | nil
---@field _pendingQuickSave                boolean
---@field _shopSession                     Source.Scenes.SceneMap.Windows.Session | nil
---@field _attrShopSession                 Source.Scenes.SceneMap.Windows.Session | nil
---@field _playerNameSession               Source.Scenes.SceneMap.Windows.Session | nil
---@field _itemMovement                    Source.Scenes.SceneMap.Windows.Movement | nil
---@field _equipMovement                   Source.Scenes.SceneMap.Windows.Movement | nil
---@field _saveLoadMovement                Source.Scenes.SceneMap.Windows.Movement | nil
---@field _enemyBookMovement               Source.Scenes.SceneMap.Windows.Movement | nil
---@field _floorTeleporterMovement         Source.Scenes.SceneMap.Windows.Movement | nil
---@field _shopRecordsMovement             Source.Scenes.SceneMap.Windows.Movement | nil
---@field _remoteShopActor                 Engine.Actor | nil
---@field private _FOCUS_GROUP_NAME        { MENU: Source.Scenes.SceneMap.Windows.FocusGroupName, ITEM: Source.Scenes.SceneMap.Windows.FocusGroupName, EQUIP_SLOT: Source.Scenes.SceneMap.Windows.FocusGroupName, EQUIP_SELECT: Source.Scenes.SceneMap.Windows.FocusGroupName, SHOP_ITEM: Source.Scenes.SceneMap.Windows.FocusGroupName, SHOP_RECORDS: Source.Scenes.SceneMap.Windows.FocusGroupName, FLOOR_COMMAND: Source.Scenes.SceneMap.Windows.FocusGroupName, FLOOR_PREVIEW: Source.Scenes.SceneMap.Windows.FocusGroupName, SAVE_SLOT: Source.Scenes.SceneMap.Windows.FocusGroupName, PLAYER_NAME: Source.Scenes.SceneMap.Windows.FocusGroupName }
---@field private _SAVE_PREVIEW_MAX_WIDTH  integer
---@field private _SAVE_PREVIEW_MAX_HEIGHT integer
local Windows = {}

---@enum Source.Scenes.SceneMap.Windows.FocusGroupName
Windows._FOCUS_GROUP_NAME = {
    MENU = "menu",
    ITEM = "item",
    EQUIP_SLOT = "equip-slot",
    EQUIP_SELECT = "equip-select",
    SHOP_ITEM = "shop-item",
    SHOP_RECORDS = "shop-records",
    FLOOR_COMMAND = "floor-command",
    FLOOR_PREVIEW = "floor-preview",
    SAVE_SLOT = "save-slot",
    PLAYER_NAME = "player-name"
}

--- Register lazy factories and focus groups without constructing windows.
---@param scene Source.Scenes.SceneMap.SceneMap
function Windows:init(scene) end

---@return boolean
function Windows:canOpenMenu() end

---@private
---@return boolean
function Windows:canOpenItemOverlay() end

---@private
---@return boolean
function Windows:isMenuBlocking() end

---@private
---@return boolean
function Windows:hasVisibleBlockingWindow() end

--- Whether a menu or another existing window blocks map input.
---@return boolean
function Windows:isBlocking() end

---@return boolean
function Windows:openMenu() end

---@private
---@param mode Enums.SaveLoadMode
---@return boolean
function Windows:openSaveLoadUI(mode) end

---@return boolean
function Windows:openSaveUI() end

---@return boolean
function Windows:openLoadUI() end

---@return boolean
function Windows:openItemUI() end

---@return boolean
function Windows:openEquipUI() end

---@return boolean
function Windows:quickSave() end

---@return boolean
function Windows:quickLoad() end

--- Capture the rendered map before committing pending menu, save-window or quick-save requests.
function Windows:afterRender() end

---@return boolean
function Windows:showEnemyBook() end

---@return boolean
function Windows:showFloorTeleporter() end

---@return boolean
function Windows:showShopRecords() end

---@private
function Windows:onShopRecordsClose() end

---@private
---@param mapPath string
---@param tag     string
function Windows:onShopRecordConfirm(mapPath, tag) end

---@return Engine.AsyncOperation
function Windows:openPlayerName() end

---@param buyItemIDs string[]
---@param canSell    boolean
---@param actor?     Engine.Actor
---@return Engine.AsyncOperation
function Windows:openShop(buyItemIDs, canSell, actor) end

---@param actor           Engine.Actor
---@param shopName        string
---@param shopDescription string
---@param abilities       table<string, integer>
---@param priceRef        GlobalFunctions.Utils.NodeReference<integer | integer[]>
---@param priceIncrement  integer
---@param moneyName       string
---@return Engine.AsyncOperation
function Windows:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName) end

---@private
function Windows:onShopClose() end

---@private
function Windows:onAttrShopClose() end

---@private
function Windows:onPlayerNameClose() end

---@private
function Windows:onItemClose() end

---@private
function Windows:onEquipClose() end

---@private
function Windows:onHotkeyItemUsed() end

---@private
function Windows:onEnemyBookClose() end

---@private
---@param entry Source.Windows.WindowEnemyBook.Entry
function Windows:onEnemyBookConfirm(entry) end

---@private
function Windows:onEnemyEncyclopediaClose() end

---@private
function Windows:onFloorTeleporterClose() end

---@private
---@param mapKey    string
---@param telepoint sf.Vector2u
function Windows:onFloorTeleporterConfirm(mapKey, telepoint) end

---@private
---@param reason Enums.SaveLoadCloseReason
function Windows:onSaveLoadClose(reason) end

---@private
function Windows:onConfigClose() end

--- Cancel active shop and name operations, dismissing only their existing windows.
function Windows:cancelAsyncOperations() end

--- Discard requests that have not yet reached the render boundary.
function Windows:cancelPendingRequests() end

--- Bind existing player-dependent windows to the host's current player without creating unused windows.
function Windows:rebindPlayer() end

--- Refresh existing windows in place while retaining selection and focus.
function Windows:refreshLocale() end

--- Cancel sessions, release existing windows and discard unused factories.
function Windows:dispose() end

return Windows
