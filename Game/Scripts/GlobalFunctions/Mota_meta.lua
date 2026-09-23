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
        CenterSymmetricTeleport = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "return",
                ["return"] = "int",
            },
            ExecSplit = {
                "Success",
                "Failed",
                Success = {
                    0,
                },
                Failed = {
                    1,
                },
            },
        },
        GoUpstairsSamePos = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "return",
                ["return"] = "int",
            },
            ExecSplit = {
                "Success",
                "Failed",
                Success = {
                    0,
                },
                Failed = {
                    1,
                },
            },
        },
        GoDownstairsSamePos = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "return",
                ["return"] = "int",
            },
            ExecSplit = {
                "Success",
                "Failed",
                Success = {
                    0,
                },
                Failed = {
                    1,
                },
            },
        },
    },
}

return _METADATA
