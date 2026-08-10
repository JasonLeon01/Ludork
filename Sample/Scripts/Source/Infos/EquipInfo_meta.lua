local _METADATA = {
    EquipInfo = {
        attrs = {},
        bases = {
            { "Engine", "InfoBase" },
        },
        onEquip = {
            type = "event",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        onUnequip = {
            type = "event",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
    },
}

return _METADATA
