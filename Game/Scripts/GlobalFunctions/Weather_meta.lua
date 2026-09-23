local _METADATA = {
    Weather = {
        attrs = {},
        SetWeather = {
            type = "function",
            parameters = {
                "weatherType",
                "power",
                "maxCount",
                weatherType = "string",
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
            },
            Meta = {
                DropBox = {
                    weatherType = {
                        "LOC(\"WEATHER_TYPE_NONE\")",
                        "LOC(\"WEATHER_TYPE_RAIN\")",
                        "LOC(\"WEATHER_TYPE_STORM\")",
                        "LOC(\"WEATHER_TYPE_SNOW\")"
                    }
                }
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
