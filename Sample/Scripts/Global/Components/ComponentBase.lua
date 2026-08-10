local ComponentBase = {}

function ComponentBase:init(parent)
    self._parent = parent
end

function ComponentBase:onTick(deltaTime)
end

function ComponentBase:onLateTick(deltaTime)
end

function ComponentBase:onFixedTick(fixedDelta)
end

function ComponentBase:onRender(camera)
end

return class(ComponentBase)
