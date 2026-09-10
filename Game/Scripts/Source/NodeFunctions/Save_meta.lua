local _METADATA = {
    Save = {
        attrs = {},
        SaveGame = {
            type = "function",
            parameters = {
                "filePath",
                filePath = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        LoadGame = {
            type = "function",
            parameters = {
                "filePath",
                filePath = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "return",
                ["return"] = "int",
            },
            ExecSplit = {
                "Loaded",
                "NotFound",
                Loaded = {
                    0,
                },
                NotFound = {
                    1,
                },
            },
        },
        GetSavePath = {
            type = "function",
            parameters = {
                "slot",
                slot = "int",
            },
            default = {
                [1] = 1,
            },
            ["return"] = {
                "path",
                path = "string",
            },
            Pure = true,
        },
    },
}

return _METADATA
