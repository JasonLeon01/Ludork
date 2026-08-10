local _METADATA = {
    State = {
        attrs = {},
        GetStateOwner = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "battler",
                battler = { "Source.Battler", "Battler" },
            },
            Pure = true,
        },
        GetEventArg = {
            type = "function",
            parameters = {
                "name",
                "default",
                name = "string",
                default = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        GetBattlerAttr = {
            type = "function",
            parameters = {
                "battler",
                "attrName",
                "default",
                battler = { "Source.Battler", "Battler" },
                attrName = "string",
                default = "any",
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        SetBattlerAttr = {
            type = "function",
            parameters = {
                "battler",
                "attrName",
                "value",
                battler = { "Source.Battler", "Battler" },
                attrName = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        DamageBattler = {
            type = "function",
            parameters = {
                "battler",
                "amount",
                battler = { "Source.Battler", "Battler" },
                amount = "int",
            },
            default = {
                [2] = 1,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        HealBattler = {
            type = "function",
            parameters = {
                "battler",
                "amount",
                battler = { "Source.Battler", "Battler" },
                amount = "int",
            },
            default = {
                [2] = 1,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        BattlerHasState = {
            type = "function",
            parameters = {
                "battler",
                "stateID",
                battler = { "Source.Battler", "Battler" },
                stateID = "string",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    {
                        "stateID",
                        "State",
                    },
                },
            },
        },
        AddStateTo = {
            type = "function",
            parameters = {
                "battler",
                "stateID",
                "stacks",
                battler = { "Source.Battler", "Battler" },
                stateID = "string",
                stacks = "int",
            },
            default = {
                [3] = 1,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    {
                        "stateID",
                        "State",
                    },
                },
            },
        },
        RemoveStateFrom = {
            type = "function",
            parameters = {
                "battler",
                "stateID",
                battler = { "Source.Battler", "Battler" },
                stateID = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    {
                        "stateID",
                        "State",
                    },
                },
            },
        },
        ReduceStateFrom = {
            type = "function",
            parameters = {
                "battler",
                "stateID",
                "stacks",
                battler = { "Source.Battler", "Battler" },
                stateID = "string",
                stacks = "int",
            },
            default = {
                [3] = 1,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    {
                        "stateID",
                        "State",
                    },
                },
            },
        },
    },
}

return _METADATA
