local Container = {}

---@generic T
---@param list  T[]
---@param index integer
---@return integer
local function listIndex(list, index)
    index = index or 0
    if index < 0 then
        index = #list + index
    end
    if index < 0 or index >= #list then
        error("list index out of range")
    end
    return index + 1
end

function Container.ForLoop(firstIndex, lastIndex, step)
    firstIndex = firstIndex == nil and 0 or firstIndex
    lastIndex = lastIndex == nil and 0 or lastIndex
    step = step == nil and 1 or step
    return firstIndex, lastIndex, step
end

function Container.ForEach(list_)
    return list_
end

function Container.CreateDict()
    return {}
end

function Container.DictGet(dict_, key)
    if Class.isInstance(dict_, dict) then
        return dict_:get(key)
    end
    return dict_[key]
end

function Container.DictAdd(dict_, key, value)
    dict_[key] = value
end

function Container.DictRemove(dict_, key)
    if Class.isInstance(dict_, dict) then
        dict_:pop(key)
        return
    end
    if dict_[key] == nil then
        error("dictionary key not found: " .. tostring(key))
    end
    dict_[key] = nil
end

function Container.DictClear(dict_)
    if Class.isInstance(dict_, dict) then
        dict_:clear()
        return
    end
    for key in pairs(dict_) do
        dict_[key] = nil
    end
end

function Container.DictContains(dict_, key)
    if Class.isInstance(dict_, dict) then
        return dict_:contains(key)
    end
    return dict_[key] ~= nil
end

function Container.TableToDict(table_)
    return dict(table_)
end

function Container.DictToTable(dict_)
    return dict_:toTable()
end

function Container.CreateList()
    return {}
end

function Container.ListGet(list_, index)
    return list_[listIndex(list_, index)]
end

function Container.ListAppend(list_, value)
    if Class.isInstance(list_, list) then
        list_:append(value)
        return
    end
    list_[#list_ + 1] = value
end

function Container.ListExtend(list_, values)
    if Class.isInstance(list_, list) then
        list_:extend(values)
        return
    end
    local length = #values
    for index = 1, length do
        list_[#list_ + 1] = values[index]
    end
end

function Container.ListRemove(list_, index)
    local resolvedIndex = listIndex(list_, index)
    if Class.isInstance(list_, list) then
        list_:pop(resolvedIndex)
        return
    end
    table.remove(list_, resolvedIndex)
end

function Container.ListFind(list_, value)
    local index
    if Class.isInstance(list_, list) then
        if list_:contains(value) then
            index = list_:index(value)
        end
    else
        index = table.index(list_, value)
    end
    if index ~= nil then
        return index - 1
    end
    error(tostring(value) .. " is not in list")
end

function Container.ListClear(list_)
    if Class.isInstance(list_, list) then
        list_:clear()
        return
    end
    for index = #list_, 1, -1 do
        list_[index] = nil
    end
end

function Container.ListContains(list_, value)
    if Class.isInstance(list_, list) then
        return list_:contains(value)
    end
    return table.contains(list_, value)
end

function Container.TableToList(table_)
    return list(table_)
end

function Container.ListToTable(list_)
    return list_:toTable()
end

return Container
