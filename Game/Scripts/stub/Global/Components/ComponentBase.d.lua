---@meta Global.Components.ComponentBase
---@class ComponentBase
---@field _parent GameMap
local ComponentBase = {}

--- @brief Base class for attachable gameplay components.
---
--- Components are attached to game objects (actors) and provide
--- reusable functionality through lifecycle callbacks.

--- @brief Initialize the component with a parent object.
--- - parent: The parent game object that this component is attached to.
---@param parent GameMap
function ComponentBase:init(parent) end

--- @brief Called every frame to update component logic.
---
--- Override this method to implement per-frame logic.
--- - deltaTime: Elapsed time in seconds.
---@param deltaTime number
function ComponentBase:onTick(deltaTime) end

--- @brief Called after onTick for late-update logic.
---
--- Override this method to implement logic that should run
--- after the main tick update.
--- - deltaTime: Elapsed time in seconds.
---@param deltaTime number
function ComponentBase:onLateTick(deltaTime) end

--- @brief Called at fixed timestep for physics-like updates.
---
--- Override this method to implement logic that should run
--- at a fixed timestep.
--- - fixedDelta: Fixed timestep in seconds.
---@param fixedDelta number
function ComponentBase:onFixedTick(fixedDelta) end

--- @brief Called to render component visuals.
--- - camera: The camera to use for rendering.
---@param camera GlobalCore.Camera
function ComponentBase:onRender(camera) end

return ComponentBase
