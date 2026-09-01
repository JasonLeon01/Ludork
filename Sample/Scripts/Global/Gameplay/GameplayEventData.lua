local GameplayEventData = {}

GameplayEventData.instigator = nil
GameplayEventData.target = nil
GameplayEventData.eventTag = ""
GameplayEventData.payload = {}

function GameplayEventData:init(values)
    values = values or {}
    self.instigator = values.instigator
    self.target = values.target
    self.eventTag = values.eventTag or ""
    self.payload = values.payload or {}
end

return class(GameplayEventData)
