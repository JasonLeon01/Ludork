local _METADATA = {
    KeyDoor = {
        attrs = {
            "needKeyID",
            "needKeyCount",
        },
        needKeyID = {
            type = "string",
            default = "",
            Meta = {
                GeneralDataVars = "Item",
            },
        },
        needKeyCount = {
            type = "int",
            default = 1,
        },
        Meta = {
            GeneralDataVars = {
                { "needKeyID", "Item" },
            },
        },
    },
}

return _METADATA
