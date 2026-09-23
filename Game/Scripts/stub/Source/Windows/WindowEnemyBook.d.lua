---@meta

---@class Source.Windows.WindowEnemyBook.SpecialDisplay
---@field texture    sf.Texture | nil
---@field nameSource string
---@field name       string

---@class Source.Windows.WindowEnemyBook.SpecialDetail
---@field specialID  string
---@field value      any
---@field nameSource string
---@field descSource string
---@field name       string
---@field desc       string

---@class Source.Windows.WindowEnemyBook.Entry
---@field nameSource      string
---@field descSource      string | nil
---@field name            string
---@field desc            string
---@field MAXHP           integer
---@field ATK             integer
---@field DEF             integer
---@field EXP             integer
---@field GOLD            integer
---@field damage          integer | string
---@field critical        GlobalCore.GameplayAbilityResult
---@field hitCount        integer | nil
---@field specialDisplays Source.Windows.WindowEnemyBook.SpecialDisplay[]
---@field specialDetails  Source.Windows.WindowEnemyBook.SpecialDetail[]
---@field texture         sf.Texture
---@field texturePath     string
---@field rect            sf.IntRect
---@field scale           sf.Vector2f
---@field animatable      boolean
---@field switchInterval  number
---@field shaderPath      string
---@field hue             number

---@class Source.Windows.WindowEnemyBook.Controller: Internal.UIBase.UiController
---@field ui                 Internal.UI.WindowEnemyBook
---@field _player            Source.MapActors.Player.Player
---@field _onCloseCallback   function | nil
---@field _onConfirmCallback function | nil
---@field _enemies           Source.Windows.WindowEnemyBook.Entry[]
---@field _cells             Internal.UIBase.UiCollection<Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller>
---@field host               Source.Windows.WindowEnemyBook
local Controller = {}

---@brief Construct the enemy handbook window.
---
--- - @param player Player used to calculate displayed damage.
--- - @param onClose Optional callback invoked when the window closes.
--- - @param onConfirm Optional callback invoked when an enemy is confirmed.
---@param player    Source.MapActors.Player.Player
---@param onClose   function | nil
---@param onConfirm function | nil
function Controller:init(player, onClose, onConfirm) end

---@brief Rebind the player used for damage preview.
---
--- - @param player The current player instance.
---@param player Source.MapActors.Player.Player
function Controller:setPlayer(player) end

---@brief Open the handbook, rescan current-map enemies, and select the first entry.
---
--- - @param gameMap Current map to scan.
---@param gameMap GameMap | nil
function Controller:open(gameMap) end

---@brief Close the handbook.
---@param onHidden function | nil
function Controller:close(onHidden) end

---@brief Refresh localised enemy, stat, and special text without rebuilding rows or previews.
function Controller:refreshLocale() end

---@brief Close the handbook through its cancel path.
function Controller:onReturn() end

---@return Source.MapActors.Player.Player
function Controller:getPlayer() end

---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller:confirmEnemy(entry) end

---@param gameMap GameMap | nil
function Controller:refreshEnemies(gameMap) end

---@param enemy  Source.MapActors.Enemy
---@param visual Global.Utils.Render.ActorVisual | nil
---@return Source.Windows.WindowEnemyBook.Entry
function Controller:buildEntry(enemy, visual) end

---@param special table
---@return Source.Windows.WindowEnemyBook.SpecialDisplay[]
function Controller:buildSpecialDisplays(special) end

---@param special table
---@return Source.Windows.WindowEnemyBook.SpecialDetail[]
function Controller:buildSpecialDetails(special) end

---@param name string
---@return string
function Controller:formatName(name) end

---@param text string | nil
---@return string
function Controller:formatText(text) end

---@param index integer
---@return sf.FloatRect
function Controller:getSelectionLayoutRect(index) end

---@param text string | nil
---@return string
function Controller.FormatLocaleText(text) end

---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller.RefreshEntryLocale(entry) end
