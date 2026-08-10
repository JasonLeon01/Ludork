---@meta Source.NodeFunctions.Container

---@param firstIndex integer
---@param lastIndex  integer
---@param step       integer
---@return integer, integer, integer
function Container.ForLoop(firstIndex, lastIndex, step) end

---@generic T
---@param list_ T[]|list<T>
---@return T[]|list<T>
function Container.ForEach(list_) end

---@generic K, V
---@return table<K, V>
function Container.CreateDict() end

---@generic K, V
---@param dict_ table<K, V>|dict<K, V>
---@param key   K
---@return V
function Container.DictGet(dict_, key) end

---@generic K, V
---@param dict_ table<K, V>|dict<K, V>
---@param key   K
---@param value V
function Container.DictAdd(dict_, key, value) end

---@generic K, V
---@param dict_ table<K, V>|dict<K, V>
---@param key   K
function Container.DictRemove(dict_, key) end

---@generic K, V
---@param dict_ table<K, V>|dict<K, V>
function Container.DictClear(dict_) end

---@generic K, V
---@param dict_ table<K, V>|dict<K, V>
---@param key   K
---@return boolean
function Container.DictContains(dict_, key) end

---@generic K, V
---@param table_ table<K, V>
---@return dict<K, V>
function Container.TableToDict(table_) end

---@param dict_ dict<any, any>
---@return table<any, any>
function Container.DictToTable(dict_) end

---@generic T
---@return T[]
function Container.CreateList() end

---@generic T
---@param list_ T[]|list<T>
---@param index integer
---@return T
function Container.ListGet(list_, index) end

---@generic T
---@param list_ T[]|list<T>
---@param value T
function Container.ListAppend(list_, value) end

---@generic T
---@param list_ T[]|list<T>
---@param values T[]|list<T>|tuple<T>
function Container.ListExtend(list_, values) end

---@generic T
---@param list_ T[]|list<T>
---@param index integer
function Container.ListRemove(list_, index) end

---@generic T
---@param list_ T[]|list<T>
---@param value T
---@return integer
function Container.ListFind(list_, value) end

---@generic T
---@param list_ T[]|list<T>
function Container.ListClear(list_) end

---@generic T
---@param list_ T[]|list<T>
---@param value T
---@return boolean
function Container.ListContains(list_, value) end

---@generic T
---@param table_ table<integer, T>
---@return list<T>
function Container.TableToList(table_) end

---@param list_ list<any>
---@return table<integer, any>
function Container.ListToTable(list_) end

return Container
