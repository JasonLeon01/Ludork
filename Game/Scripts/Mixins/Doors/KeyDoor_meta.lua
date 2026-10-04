local _METADATA = {
    KeyDoor = {
        attrs = {
            "needKeyID",
            "needKeyCount"
        },
        needKeyID = {
            type = { enum = "Enums.GeneralData.Item", valueType = "string" },
            default = ""
        },
        needKeyCount = {
            type = "int",
            default = 1
        }
    }
}

return _METADATA
