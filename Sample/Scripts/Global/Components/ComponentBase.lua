local ComponentBase = {}

function ComponentBase:init(parent)
    self._parent = parent
end

function ComponentBase:onTick(deltaTime)
    local _self, _deltaTime = self, deltaTime
end

function ComponentBase:onLateTick(deltaTime)
    local _self, _deltaTime = self, deltaTime
end

function ComponentBase:onFixedTick(fixedDelta)
    local _self, _fixedDelta = self, fixedDelta
end

function ComponentBase:onRender(camera)
    local _self, _camera = self, camera
end

return class(ComponentBase)
