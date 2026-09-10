local ComponentBase = {}

function ComponentBase:init(parent)
    self._parent = parent
end

---@diagnostic disable-next-line: unused
function ComponentBase:onTick(deltaTime)
end

---@diagnostic disable-next-line: unused
function ComponentBase:onLateTick(deltaTime)
end

---@diagnostic disable-next-line: unused
function ComponentBase:onFixedTick(fixedDelta)
end

---@diagnostic disable-next-line: unused
function ComponentBase:onRender(camera)
end

return class(ComponentBase)
