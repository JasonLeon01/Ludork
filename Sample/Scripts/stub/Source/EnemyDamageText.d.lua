---@meta Source.EnemyDamageText

---@alias Source.EnemyDamageText.DamageHintLevel integer
local DamageHintLevel = {}

---@brief Text-only child actor showing enemy damage against the player.
---
--- `EnemyDamageHintLevel` defaults to `DamageHintLevel.BATTLE`. `NONE` hides expected-damage hints,
--- `BATTLE` shows enemy battle readouts, and `MAP` additionally shows movement-danger cell totals.
--- The configured `requiredItemID` remains a prerequisite at every visible level. Actual damage
--- particles shown after HP loss are not hints and are not suppressed by this setting.
---@class Source.EnemyDamageText: Engine.Actor
---@field DamageHintLevel        { NONE: Source.EnemyDamageText.DamageHintLevel, BATTLE: Source.EnemyDamageText.DamageHintLevel, MAP: Source.EnemyDamageText.DamageHintLevel }
---@field EnemyDamageHintLevel   Source.EnemyDamageText.DamageHintLevel
---@field requiredItemID         string
---@field textConfig             string
---@field damageTextOffset       sf.Vector2f
---@field _text                  Engine.PlainText?
---@field _textRenderStates      sf.RenderStates
---@field _textDisplayScale      number | nil
---@field _overlayTexture        sf.Texture?
---@field _overlayTextureWidth   integer
---@field _overlayTextureHeight  integer
---@field _blankTexture          sf.Texture?
---@field _scratchRenderTexture  sf.RenderTexture?
---@field _scratchWidth          integer
---@field _scratchHeight         integer
---@field _currentDamageText     string
---@field _currentCriticalText   string
---@field _currentDamageColorR   integer | nil
---@field _currentDamageColorG   integer | nil
---@field _currentDamageColorB   integer | nil
---@field _currentDamageColorA   integer | nil
---@field _currentOverlayWidth   integer | nil
---@field _currentOverlayHeight  integer | nil
---@field _currentBattlers       table<integer, Source.Battler.Battler>
---@field _currentParentRevision integer | nil
---@field _currentPlayerRevision integer | nil
---@field _currentOffsetX        number | nil
---@field _currentOffsetY        number | nil
---@field _overlayVisible        boolean
---@field _renderDirty           boolean
---@field _fillColor             sf.Color
local EnemyDamageText = {}

---@brief Construct an enemy damage text actor.
---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function EnemyDamageText:init(texture, rect, tag) end

---@brief Refresh damage text and visibility.
---
--- - @param deltaTime Time elapsed since the last frame.
---@param deltaTime number
function EnemyDamageText:onTick(deltaTime) end

---@brief Resolve the shared colour used for an expected damage value.
---@param damage   integer | nil
---@param playerHP integer
---@return sf.Color
function EnemyDamageText.GetDamageColor(damage, playerHP) end

return EnemyDamageText
