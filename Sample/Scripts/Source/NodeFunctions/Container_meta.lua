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
                step = "int"
            },
            default = { 0, 0, 1 },
            ["return"] = {
                "index",
                index = "int"
            },
            ExecSplit = {
                "LoopBody",
                "Completed",
                LoopBody = { "__loop_body__" },
                Completed = { "__loop_completed__" }
            },
            LoopNode = "ForLoop"
        },
        ForEach = {
            type = "function",
            parameters = {
                "list_",
                list_ = { union = { "any[]", { "_G", "list" } } }
            },
            ["return"] = {
                "element",
                "index",
                element = "any",
                index = "int"
            },
            ExecSplit = {
                "LoopBody",
                "Completed",
                LoopBody = { "__loop_body__" },
                Completed = { "__loop_completed__" }
            },
            LoopNode = "ForEach"
        },
        CreateDict = {
            type = "function",
            parameters = {},
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        DictGet = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                dict_ = "any",
                key = "any"
            },
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        DictAdd = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                "value",
                dict_ = "any",
                key = "any",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        DictRemove = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                dict_ = "any",
                key = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        DictClear = {
            type = "function",
            parameters = {
                "dict_",
                dict_ = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        DictContains = {
            type = "function",
            parameters = {
                "dict_",
                "key",
                dict_ = "any",
                key = "any"
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true
        },
        TableToDict = {
            type = "function",
            parameters = {
                "table_",
                table_ = "any"
            },
            ["return"] = {
                "value",
                value = { "_G", "dict" }
            },
            Pure = true
        },
        DictToTable = {
            type = "function",
            parameters = {
                "dict_",
                dict_ = { "_G", "dict" }
            },
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        CreateList = {
            type = "function",
            parameters = {},
            ["return"] = {
                "value",
                value = "any[]"
            },
            Pure = true
        },
        ListGet = {
            type = "function",
            parameters = {
                "list_",
                "index",
                list_ = { union = { "any[]", { "_G", "list" }, { "_G", "tuple" } } },
                index = "int"
            },
            default = {
                [2] = 0
            },
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        ListAppend = {
            type = "function",
            parameters = {
                "list_",
                "value",
                list_ = { union = { "any[]", { "_G", "list" } } },
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ListExtend = {
            type = "function",
            parameters = {
                "list_",
                "values",
                list_ = { union = { "any[]", { "_G", "list" } } },
                values = { union = { "any[]", { "_G", "list" }, { "_G", "tuple" } } }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ListRemove = {
            type = "function",
            parameters = {
                "list_",
                "index",
                list_ = { union = { "any[]", { "_G", "list" } } },
                index = "int"
            },
            default = {
                [2] = 0
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ListFind = {
            type = "function",
            parameters = {
                "list_",
                "value",
                list_ = { union = { "any[]", { "_G", "list" } } },
                value = "any"
            },
            ["return"] = {
                "index",
                index = "int"
            },
            Pure = true
        },
        ListClear = {
            type = "function",
            parameters = {
                "list_",
                list_ = { union = { "any[]", { "_G", "list" } } }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ListContains = {
            type = "function",
            parameters = {
                "list_",
                "value",
                list_ = { union = { "any[]", { "_G", "list" } } },
                value = "any"
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true
        },
        TableToList = {
            type = "function",
            parameters = {
                "table_",
                table_ = "any[]"
            },
            ["return"] = {
                "value",
                value = { "_G", "list" }
            },
            Pure = true
        },
        ListToTable = {
            type = "function",
            parameters = {
                "list_",
                list_ = { "_G", "list" }
            },
            ["return"] = {
                "value",
                value = "any[]"
            },
            Pure = true
        }
    }
}

return _METADATA
