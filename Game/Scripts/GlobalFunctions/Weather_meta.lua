local _METADATA = {
    Weather = {
        attrs = {},
        SetWeather = {
            type = "function",
            parameters = {
                "weatherType",
                "power",
                "maxCount",
                weatherType = { enum = "Enums.GlobalCore.WeatherType" },
                power = "int",
                maxCount = "int"
            },
            default = {
                [2] = 40,
                [3] = 80
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ClearWeather = {
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
