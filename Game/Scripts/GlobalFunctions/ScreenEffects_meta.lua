local _METADATA = {
    ScreenEffects = {
        attrs = {},
        FlashScreen = {
            type = "function",
            parameters = {
                "red",
                "green",
                "blue",
                "alpha",
                "duration",
                red = "int",
                green = "int",
                blue = "int",
                alpha = "int",
                duration = "float"
            },
            default = {
                [1] = 255,
                [2] = 255,
                [3] = 255,
                [4] = 255,
                [5] = 0.5
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        StopFlashScreen = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ChangeScreenTone = {
            type = "function",
            parameters = {
                "red",
                "green",
                "blue",
                "gray",
                "duration",
                red = "int",
                green = "int",
                blue = "int",
                gray = "int",
                duration = "float"
            },
            default = {
                [1] = 0,
                [2] = 0,
                [3] = 0,
                [4] = 0,
                [5] = 0.0
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ClearScreenTone = {
            type = "function",
            parameters = {
                "duration",
                duration = "float"
            },
            default = {
                [1] = 0.0
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ScreenShake = {
            type = "function",
            parameters = {
                "power",
                "speed",
                "duration",
                power = "float",
                speed = "float",
                duration = "float"
            },
            default = {
                [1] = 4.0,
                [2] = 10.0,
                [3] = 0.5
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        StopScreenShake = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        }
    }
}

return _METADATA
