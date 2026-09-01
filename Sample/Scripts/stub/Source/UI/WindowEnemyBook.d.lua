---@meta Source.UI.WindowEnemyBook

---@class Source.UI.WindowEnemyBook.SpecialDisplay
---@field texture    sf.Texture | nil
---@field nameSource string
---@field name       string

---@class Source.UI.WindowEnemyBook.SpecialDetail
---@field nameSource string
---@field descSource string
---@field name       string
---@field desc       string

---@class Source.UI.WindowEnemyBook.Entry
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
---@field critical        Global.Gameplay.GameplayAbilityResult
---@field hitCount        integer | nil
---@field specialDisplays Source.UI.WindowEnemyBook.SpecialDisplay[]
---@field specialDetails  Source.UI.WindowEnemyBook.SpecialDetail[]
---@field visual          Global.Utils.Render.ActorVisual
---@field texture         sf.Texture
---@field texturePath     string
---@field rect            sf.IntRect | nil
---@field scale           sf.Vector2f
---@field animatable      boolean
---@field switchInterval  number

---@class Source.UI.WindowEnemyBook: Source.UI.UiController
---@field model            Source.Windows.WindowEnemyBook
---@field _size            sf.Vector2i
---@field _cellControllers Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell[]
---@field _windowFrame     Engine.Window
---@field _content         Engine.Canvas
---@field _listView        Engine.ListView
---@field new              fun(model: Source.Windows.WindowEnemyBook, size: sf.Vector2i): Source.UI.WindowEnemyBook
local WindowEnemyBookUI = {}

---@param text string | nil
---@return string
function WindowEnemyBookUI.FormatLocaleText(text) end

---@param entry Source.UI.WindowEnemyBook.Entry
function WindowEnemyBookUI.RefreshEntryLocale(entry) end

---@param model Source.Windows.WindowEnemyBook
---@param size  sf.Vector2i
function WindowEnemyBookUI:init(model, size) end

function WindowEnemyBookUI:bind() end

---@return Engine.Canvas
function WindowEnemyBookUI:prepare() end

function WindowEnemyBookUI:attach() end

---@return Engine.Window
function WindowEnemyBookUI:getWindowFrame() end

---@return Engine.Canvas
function WindowEnemyBookUI:getContent() end

---@return Engine.ListView
function WindowEnemyBookUI:getListView() end

---@param gameMap GameMap | nil
function WindowEnemyBookUI:refreshEnemies(gameMap) end

---@param deltaTime number
function WindowEnemyBookUI:tick(deltaTime) end

function WindowEnemyBookUI:refreshLocale() end

---@param enemy  Source.Enemy
---@param visual Global.Utils.Render.ActorVisual | nil
---@return Source.UI.WindowEnemyBook.Entry
function WindowEnemyBookUI:buildEntry(enemy, visual) end

---@param special table
---@return Source.UI.WindowEnemyBook.SpecialDisplay[]
function WindowEnemyBookUI:buildSpecialDisplays(special) end

---@param special table
---@return Source.UI.WindowEnemyBook.SpecialDetail[]
function WindowEnemyBookUI:buildSpecialDetails(special) end

---@param name string
---@return string
function WindowEnemyBookUI:formatName(name) end

---@param text string | nil
---@return string
function WindowEnemyBookUI:formatText(text) end

return WindowEnemyBookUI
