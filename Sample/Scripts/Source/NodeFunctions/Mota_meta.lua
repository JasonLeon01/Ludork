local _METADATA = {
    Mota = {
        attrs = {},
        OpenMonsterBook = {
            type = "function",
            parameters = {
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        OpenFloorTeleporter = {
            type = "function",
            parameters = {
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        GetCurrentRegion = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "region",
                region = "string",
            },
            Pure = true,
        },
        SetCurrentRegion = {
            type = "function",
            parameters = {
                "region",
                region = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
    },
}

return _METADATA
