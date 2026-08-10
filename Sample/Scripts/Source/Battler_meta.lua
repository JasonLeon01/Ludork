local _METADATA = {
    Battler = {
        attrs = {
            "infoComp",
        },
        infoComp = {
            type = { "Source.Components.BattlerInfoComponent", "BattlerInfoComponent" },
            component = true,
            default = {
                MAXHP = 1000,
                ATK = 10,
                DEF = 10,
                EXP = 0,
                GOLD = 0,
                ANIMATION_KEY = "",
                HP = 0,
            },
        },
        addState = {
            type = "function",
            parameters = {
                "state",
                "stacks",
                state = "string",
                stacks = "int",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    { "state", "State" },
                },
            },
        },
        removeState = {
            type = "function",
            parameters = {
                "state",
                state = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    { "state", "State" },
                },
            },
        },
        reduceStateStacks = {
            type = "function",
            parameters = {
                "state",
                "stacks",
                state = "string",
                stacks = "int",
            },
            default = {
                [2] = 1,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    { "state", "State" },
                },
            },
        },
        triggerStateWalk = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        triggerStateHook = {
            type = "function",
            parameters = {
                "stateKey",
                stateKey = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    { "stateKey", "State" },
                },
            },
        },
    },
}

return _METADATA
