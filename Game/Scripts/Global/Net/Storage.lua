local Net = require("Global.Net")
local StorageProxy = require("Global.Net.StorageProxy")

return Net.DefineModule("Storage", StorageProxy)
