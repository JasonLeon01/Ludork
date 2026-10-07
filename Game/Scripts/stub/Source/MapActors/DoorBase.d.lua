---@meta Source.MapActors.DoorBase

--- Base door actor that plays sprite-sheet open and close animations.
---
--- The texture should contain frames arranged horizontally (left to right).
--- When `openDoor()` is called, the door advances through each frame at
--- `openInterval` seconds, then self-destructs. When `closeDoor()` is called,
--- the door animates from the current frame back to the first frame. The frame
--- count is derived from the texture width, the initial rect origin, and the
--- frame width. The door is NOT `animatable` — animations are driven manually
--- in `onTick()`.
---
--- `tickable` keeps the Actor default `False`; valid open/close playback
--- enables it only until the animation completes or the Actor is destroyed.
--- Calling `openDoor()` or `closeDoor()` while the same animation is already
--- running returns its pending operation. Reversing the animation cancels the
--- previous operation. Destroying the actor cancels unfinished operations.
---@class Source.MapActors.DoorBase.DoorBase: Source.MapActors.ConditionalActor
---@field opening          boolean
---@field closing          boolean
---@field collisionEnabled boolean
---@field openInterval     number
---@field gateSE           string
---@field _doorOperation   Engine.AsyncOperation | nil
---@field _frameIndex      integer
---@field _animTimer       number
---@field _openFinished    boolean
---@field _closeFinished   boolean
---@field _frameWidth      integer
---@field _startX          integer
---@field _startY          integer
local DoorBase = {}

--- Construct a Door actor.
---
--- - @param texture  The sprite-sheet texture (frames arranged horizontally)
--- - @param rect     Sub-rectangle for the initial frame
--- - @param tag      Optional identifier tag
---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function DoorBase:init(texture, rect, tag) end

--- Set the texture sub-rectangle and keep the closed-frame layout in sync.
---
--- While the door is idle, the rect defines the animation strip origin.
--- During open/close playback the layout is held fixed so frame advances
--- do not overwrite it.
---@param rect sf.IntRect
function DoorBase:setTextureRect(rect) end

--- Start the door-open animation (latent).
---
--- Advances through each frame every `openInterval` seconds, then
--- self-destructs after completing its operation. Repeated calls while opening
--- share the operation. Destroyed actors return a cancelled operation unless
--- opening already finished. Interrupts an in-progress close animation.
---
--- - @return An operation that emits Started and completes with Finished.
---@return Engine.AsyncOperation
function DoorBase:openDoor() end

--- Start the door-close animation (latent).
---
--- Animates from the current frame back to the first frame every
--- `openInterval` seconds. Repeated calls while closing share the operation.
--- Destroyed actors return a cancelled operation; already-closed actors return
--- a completed operation. Interrupts an in-progress open animation.
---
--- - @return An operation that emits Started and completes with Finished.
---@return Engine.AsyncOperation
function DoorBase:closeDoor() end

--- Blueprint event: drive open/close animations when active.
---
--- Called every frame while an open/close animation is active.
---
--- - @param deltaTime  Seconds since the last frame
---@param deltaTime number
function DoorBase:onTick(deltaTime) end

--- Disable animation ticking before the Actor is destroyed.
function DoorBase:onDestroy() end

return DoorBase
