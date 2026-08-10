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
        onGet = {
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
