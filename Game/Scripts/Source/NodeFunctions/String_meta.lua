local _METADATA = {
    String = {
        attrs = {},
        ToString = {
            type = "function",
            parameters = {
                "value",
                value = "any",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        GetIntFromStr = {
            type = "function",
            parameters = {
                "value",
                value = "string",
            },
            default = {
                [1] = "0",
            },
            ["return"] = {
                "value",
                value = "int",
            },
            Pure = true,
        },
        GetFloatFromStr = {
            type = "function",
            parameters = {
                "value",
                value = "string",
            },
            default = {
                [1] = "0",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        StringConcat = {
            type = "function",
            parameters = {
                "str1",
                "str2",
                str1 = "string",
                str2 = "string",
            },
            default = {
                [1] = "",
                [2] = "",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        StringContains = {
            type = "function",
            parameters = {
                "str1",
                "str2",
                str1 = "string",
                str2 = "string",
            },
            default = {
                [1] = "",
                [2] = "",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        StringLength = {
            type = "function",
            parameters = {
                "str1",
                str1 = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "value",
                value = "int",
            },
            Pure = true,
        },
        StringFind = {
            type = "function",
            parameters = {
                "str1",
                "str2",
                str1 = "string",
                str2 = "string",
            },
            default = {
                [1] = "",
                [2] = "",
            },
            ["return"] = {
                "value",
                value = "int",
            },
            Pure = true,
        },
        StringReplace = {
            type = "function",
            parameters = {
                "str1",
                "str2",
                "str3",
                str1 = "string",
                str2 = "string",
                str3 = "string",
            },
            default = {
                [1] = "",
                [2] = "",
                [3] = "",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        StringSplit = {
            type = "function",
            parameters = {
                "str1",
                "str2",
                str1 = "string",
                str2 = "string",
            },
            default = {
                [1] = "",
                [2] = ",",
            },
            ["return"] = {
                "value",
                value = "string[]",
            },
            Pure = true,
        },
        StringSubstring = {
            type = "function",
            parameters = {
                "str1",
                "start",
                "end",
                str1 = "string",
                start = "int",
                ["end"] = "int",
            },
            default = {
                [1] = "",
                [2] = 0,
                [3] = 0,
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        StringToLower = {
            type = "function",
            parameters = {
                "str1",
                str1 = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        StringToUpper = {
            type = "function",
            parameters = {
                "str1",
                str1 = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        StringStrip = {
            type = "function",
            parameters = {
                "str1",
                str1 = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
    },
}

return _METADATA
