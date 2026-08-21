local _METADATA = {
    ItemInfo = {
        attrs = {},
        bases = {
            { "Engine", "InfoBase" },
        },
        onUse = {
            type = "event",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        onDrop = {
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
