local _METADATA = {
    Container = {
        attrs = {},
        ForLoop = {
            type = "function",
            parameters = {
                "firstIndex",
                "lastIndex",
                "step",
                firstIndex = "int",
                lastIndex = "int",
                step = "int",
            },
            default = { 0, 0, 1 },
            ["return"] = {
                "index",
                index = "int",
            },
            ExecSplit = {
                "LoopBody",
                "Completed",
                LoopBody = { "__loop_body__" },
                Completed = { "__loop_completed__" },
            },
            LoopNode = "ForLoop",
        },
        ForEach = {
            type = "function",
            parameters = {
                "list_",
                list_ = "any[]",
            },
            ["return"] = {
                "element",
                "index",
                element = "any",
                index = "int",
            },
            ExecSplit = {
                "LoopBody",
                "Completed",
                LoopBody = { "__loop_body__" },
                Completed = { "__loop_completed__" },
            },
            LoopNode = "ForEach",
        },
        CreateDict = {
            type = "function",
            parameters = {},
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        DictGet = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                dict_ = "any",
                key = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        DictAdd = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                "value",
                dict_ = "any",
                key = "any",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        DictRemove = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                dict_ = "any",
                key = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        DictClear = {
            type = "function",
            parameters = {
                "dict_",
                dict_ = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        DictContains = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                dict_ = "any",
                key = "any",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        TableToDict = {
            type = "function",
            parameters = {
                "table_",
                table_ = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        DictToTable = {
            type = "function",
            parameters = {
                "dict_",
                dict_ = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        CreateList = {
            type = "function",
            parameters = {},
            ["return"] = {
                "value",
                value = "any[]",
            },
            Pure = true,
        },
        ListGet = {
            type = "function",
            parameters = {
                "list_",
                "index",
                list_ = "any[]",
                index = "int",
            },
            default = {
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        ListAppend = {
            type = "function",
            parameters = {
                "list_",
                "value",
                list_ = "any[]",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ListExtend = {
            type = "function",
            parameters = {
                "list_",
                "values",
                list_ = "any[]",
                values = "any[]",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ListRemove = {
            type = "function",
            parameters = {
                "list_",
                "index",
                list_ = "any[]",
                index = "int",
            },
            default = {
                [2] = 0,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ListFind = {
            type = "function",
            parameters = {
                "list_",
                "value",
                list_ = "any[]",
                value = "any",
            },
            ["return"] = {
                "index",
                index = "int",
            },
            Pure = true,
        },
        ListClear = {
            type = "function",
            parameters = {
                "list_",
                list_ = "any[]",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ListContains = {
            type = "function",
            parameters = {
                "list_",
                "value",
                list_ = "any[]",
                value = "any",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        TableToList = {
            type = "function",
            parameters = {
                "table_",
                table_ = "any[]",
            },
            ["return"] = {
                "value",
                value = "any[]",
            },
            Pure = true,
        },
        ListToTable = {
            type = "function",
            parameters = {
                "list_",
                list_ = "any[]",
            },
            ["return"] = {
                "value",
                value = "any[]",
            },
            Pure = true,
        },
    },
}

return _METADATA
