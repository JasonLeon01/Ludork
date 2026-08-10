local _METADATA = {
    Utils = {
        attrs = {},
        IF = {
            type = "function",
            parameters = {
                "condition",
                condition = "bool",
            },
            default = {
                [1] = false,
            },
            ["return"] = {
                "return",
                ["return"] = "int",
            },
            ExecSplit = {
                "TRUE",
                "FALSE",
                TRUE = {
                    0,
                },
                FALSE = {
                    1,
                },
            },
        },
        SetLocalValue = {
            type = "function",
            parameters = {
                "valueName",
                "value",
                valueName = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        GetLocalValue = {
            type = "function",
            parameters = {
                "valueName",
                "default",
                valueName = "string",
                default = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        GetLocalValueRef = {
            type = "function",
            parameters = {
                "valueName",
                "default",
                valueName = "string",
                default = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        SetGameVariable = {
            type = "function",
            parameters = {
                "valueName",
                "value",
                valueName = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        GetGameVariable = {
            type = "function",
            parameters = {
                "valueName",
                "default",
                valueName = "string",
                default = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        GetGameVariableRef = {
            type = "function",
            parameters = {
                "valueName",
                "default",
                valueName = "string",
                default = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        AddPlayerByClass = {
            type = "function",
            parameters = {
                "playerClass",
                playerClass = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                BlueprintClassVars = {
                    "playerClass",
                },
            },
        },
        RemovePlayerByClass = {
            type = "function",
            parameters = {
                "playerClass",
                playerClass = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                BlueprintClassVars = {
                    "playerClass",
                },
            },
        },
        AddAnim = {
            type = "function",
            parameters = {
                "animName",
                "position",
                "rotation",
                "scale",
                animName = "string",
                position = "sf.Vector2f",
                rotation = "float",
                scale = "sf.Vector2f",
            },
            default = {
                [2] = {
                    0.0,
                    0.0,
                },
                [3] = 0.0,
                [4] = {
                    1.0,
                    1.0,
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    {
                        "animName",
                        "ANIMATION",
                    },
                },
            },
        },
        AddAnimOn = {
            type = "function",
            parameters = {
                "animName",
                "actorTag",
                "rotation",
                "scale",
                animName = "string",
                actorTag = "string",
                rotation = "float",
                scale = "sf.Vector2f",
            },
            default = {
                [3] = 0.0,
                [4] = {
                    1.0,
                    1.0,
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    {
                        "animName",
                        "ANIMATION",
                    },
                },
            },
        },
        GetAnimLength = {
            type = "function",
            parameters = {
                "animName",
                animName = "string",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    {
                        "animName",
                        "ANIMATION",
                    },
                },
            },
        },
        GetAnimVisualLength = {
            type = "function",
            parameters = {
                "animName",
                animName = "string",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    {
                        "animName",
                        "ANIMATION",
                    },
                },
            },
        },
        SUPER = {
            type = "function",
            parameters = {
                "obj",
                "params",
                obj = "any",
                params = "any[]",
            },
            default = {
                [2] = {},
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        SELF = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        GetAttrRef = {
            type = "function",
            parameters = {
                "obj",
                "attrName",
                obj = "any",
                attrName = "string",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        GetAttr = {
            type = "function",
            parameters = {
                "obj",
                "attrName",
                obj = "any",
                attrName = "string",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        SetAttr = {
            type = "function",
            parameters = {
                "obj",
                "attrName",
                "value",
                obj = "any",
                attrName = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        GetScene = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "value",
                value = {
                    "GlobalCore",
                    "SceneBase",
                },
            },
            Pure = true,
        },
        IsValidValue = {
            type = "function",
            parameters = {
                "value",
                value = "any",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        ToShortNumber = {
            type = "function",
            parameters = {
                "value",
                value = "any",
            },
            default = {
                [1] = 0,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        RunCommonFunction = {
            type = "function",
            parameters = {
                "commonFunctionName",
                commonFunctionName = "string",
            },
            default = {
                [1] = "",
            },
            ["return"] = {
                "return",
                ["return"] = "any",
            },
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                CommonFunctionVars = {
                    "commonFunctionName",
                },
            },
        },
        RegisterEventBus = {
            type = "function",
            parameters = {
                "key",
                "obj",
                "functionName",
                key = "string",
                obj = "any",
                functionName = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        RegisterEventBusEvent = {
            type = "function",
            parameters = {
                "key",
                "obj",
                "eventName",
                key = "string",
                obj = "any",
                eventName = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        UnregisterEventBus = {
            type = "function",
            parameters = {
                "key",
                key = "string",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        UnregisterEventBusEvent = {
            type = "function",
            parameters = {
                "key",
                "obj",
                key = "string",
                obj = "any",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        TriggerEventBus = {
            type = "function",
            parameters = {
                "key",
                "kwargs",
                key = "string",
                kwargs = "any",
            },
            default = {
                [2] = {},
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        TriggerBlueprintEvent = {
            type = "function",
            parameters = {
                "obj",
                "eventName",
                obj = "any",
                eventName = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        BackToTitle = {
            type = "function",
            parameters = {
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        Print = {
            type = "function",
            parameters = {
                "message",
                message = "any",
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
        EXEC = {
            type = "function",
            parameters = {
                "script",
                script = "string",
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
        GetSelfAttr = {
            type = "function",
            parameters = {
                "attrName",
                attrName = "string",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        SetSelfAttr = {
            type = "function",
            parameters = {
                "attrName",
                "value",
                attrName = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        IfPlayerOverlaps = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        IfGameVar = {
            type = "function",
            parameters = {
                "varName",
                "op",
                "value",
                varName = "string",
                op = "string",
                value = "any",
            },
            default = {
                [1] = "",
                [2] = "==",
            },
            ["return"] = {
                "return",
                ["return"] = "bool",
            },
            ExecSplit = {
                "TRUE",
                "FALSE",
                TRUE = {
                    true,
                },
                FALSE = {
                    false,
                },
            },
            Meta = {
                DropBox = {
                    op = {
                        "==",
                        "!=",
                        "<",
                        "<=",
                        ">",
                        ">=",
                    },
                },
            },
        },
    },
}

return _METADATA
