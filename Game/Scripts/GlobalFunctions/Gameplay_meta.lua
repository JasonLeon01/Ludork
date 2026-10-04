local _METADATA = {
    Gameplay = {
        attrs = {},
        GetContext = {
            type = "function",
            parameters = {},
            ["return"] = {
                "context",
                context = { "GlobalCore", "GameplayEventData" }
            },
            Pure = true
        },
        GetSource = {
            type = "function",
            parameters = {},
            ["return"] = {
                "source",
                source = "any"
            },
            Pure = true
        },
        GetTarget = {
            type = "function",
            parameters = {},
            ["return"] = {
                "target",
                target = "any"
            },
            Pure = true
        },
        GetEventTag = {
            type = "function",
            parameters = {},
            ["return"] = {
                "eventTag",
                eventTag = "string"
            },
            Pure = true
        },
        GetPayload = {
            type = "function",
            parameters = {},
            ["return"] = {
                "payload",
                payload = "any"
            },
            Pure = true
        },
        HasTag = {
            type = "function",
            parameters = {
                "target",
                "tag",
                target = { "Source.Battler", "Battler" },
                tag = "string"
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true
        },
        GetNumericAttribute = {
            type = "function",
            parameters = {
                "target",
                "attribute",
                target = { "Source.Battler", "Battler" },
                attribute = "string"
            },
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        SetNumericAttributeBase = {
            type = "function",
            parameters = {
                "target",
                "attribute",
                "value",
                target = { "Source.Battler", "Battler" },
                attribute = "string",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ApplyAttributeDelta = {
            type = "function",
            parameters = {
                "target",
                "attribute",
                "magnitude",
                target = { "Source.Battler", "Battler" },
                attribute = "string",
                magnitude = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ApplyState = {
            type = "function",
            parameters = {
                "target",
                "stateID",
                "stacks",
                target = { "Source.Battler", "Battler" },
                stateID = { enum = "Enums.GeneralData.State", valueType = "string" },
                stacks = "int"
            },
            default = {
                [3] = 1
            },
            ["return"] = {
                "handle",
                handle = "int"
            }
        },
        RemoveState = {
            type = "function",
            parameters = {
                "target",
                "stateID",
                target = { "Source.Battler", "Battler" },
                stateID = { enum = "Enums.GeneralData.State", valueType = "string" }
            },
            ["return"] = {},
            ExecSplit = {
                "success",
                "failed",
                success = true,
                failed = false
            }
        },
        ReduceState = {
            type = "function",
            parameters = {
                "target",
                "stateID",
                "stacks",
                target = { "Source.Battler", "Battler" },
                stateID = { enum = "Enums.GeneralData.State", valueType = "string" },
                stacks = "int"
            },
            default = {
                [3] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "success",
                "failed",
                success = true,
                failed = false
            }
        },
        RemovePlayerState = {
            type = "function",
            parameters = {
                "stateID",
                stateID = { enum = "Enums.GeneralData.State", valueType = "string" }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        ReducePlayerState = {
            type = "function",
            parameters = {
                "stateID",
                "stacks",
                stateID = { enum = "Enums.GeneralData.State", valueType = "string" },
                stacks = "int"
            },
            default = {
                [2] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        SendEvent = {
            type = "function",
            parameters = {
                "target",
                "eventTag",
                "payload",
                target = { "Source.Battler", "Battler" },
                eventTag = "string",
                payload = "any"
            },
            default = {
                [3] = {}
            },
            ["return"] = {
                "results",
                results = "any"
            }
        },
        ApplyEffect = {
            type = "function",
            parameters = {
                "target",
                "effect",
                "stacks",
                "sourceKey",
                target = { "Source.Battler", "Battler" },
                effect = "any",
                stacks = "int",
                sourceKey = "any"
            },
            default = {
                [3] = 1
            },
            ["return"] = {
                "handle",
                handle = "int"
            }
        }
    }
}

return _METADATA
