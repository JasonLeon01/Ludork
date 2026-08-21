local _METADATA = {
    Player = {
        attrs = {
            "ID",
            "tickable",
            "collisionEnabled",
            "animatable",
            "speed",
            "infoComp",
        },
        bases = {
            { "Engine", "Character" },
            { "Source.Infos.PlayerInfo", "PlayerInfo" },
            { "Source.Battler", "Battler" },
        },
        ID = {
            type = "string",
            default = "FILL_IT_BY_YOURSELF",
            Meta = {
                GeneralDataVars = "Player",
            },
        },
        tickable = {
            type = "bool",
            default = true,
        },
        collisionEnabled = {
            type = "bool",
            default = true,
        },
        animatable = {
            type = "bool",
            default = true,
        },
        speed = {
            type = "float",
            default = 96.0,
        },
        infoComp = {
            type = { "Source.Components.PlayerInfoComponent", "PlayerInfoComponent" },
            component = true,
            default = {
                MAXHP = 1000,
                ATK = 10,
                DEF = 10,
                EXP = 0,
                GOLD = 0,
                ANIMATION_KEY = "",
                HP = 0,
                name = "",
                desc = "",
                LEVEL = 1,
                CLASS = "",
            },
        },
        onFixedTick = {
            type = "event",
            parameters = {
                "fixedDelta",
                fixedDelta = "float",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        addItem = {
            type = "function",
            parameters = {
                "itemID",
                "count",
                itemID = "string",
                count = "int",
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
                    { "itemID", "Item" },
                },
            },
        },
        removeItem = {
            type = "function",
            parameters = {
                "itemID",
                "count",
                itemID = "string",
                count = "int",
            },
            default = {
                [2] = 1,
            },
            ["return"] = {
                "return",
                ["return"] = "bool",
            },
            ExecSplit = {
                "success",
                "failed",
                success = { true },
                failed = { false },
            },
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" },
                },
            },
        },
        getItemCount = {
            type = "function",
            parameters = {
                "itemID",
                itemID = "string",
            },
            ["return"] = {
                "count",
                count = "int",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" },
                },
            },
        },
        hasItem = {
            type = "function",
            parameters = {
                "itemID",
                itemID = "string",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" },
                },
            },
        },
        addEquip = {
            type = "function",
            parameters = {
                "equipID",
                "count",
                equipID = "string",
                count = "int",
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
                    { "equipID", "Equip" },
                },
            },
        },
        removeEquip = {
            type = "function",
            parameters = {
                "equipID",
                "count",
                equipID = "string",
                count = "int",
            },
            default = {
                [2] = 1,
            },
            ["return"] = {
                "return",
                ["return"] = "bool",
            },
            ExecSplit = {
                "success",
                "failed",
                success = { true },
                failed = { false },
            },
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" },
                },
            },
        },
        equip = {
            type = "function",
            parameters = {
                "equipID",
                equipID = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" },
                },
            },
        },
        unequip = {
            type = "function",
            parameters = {
                "slotID",
                slotID = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        getEquipCount = {
            type = "function",
            parameters = {
                "equipID",
                equipID = "string",
            },
            ["return"] = {
                "count",
                count = "int",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" },
                },
            },
        },
        hasEquip = {
            type = "function",
            parameters = {
                "equipID",
                equipID = "string",
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" },
                },
            },
        },
        getEquipInfo = {
            type = "function",
            parameters = {
                "slotID",
                slotID = "string",
            },
            ["return"] = {
                "value",
                value = "string",
            },
            Pure = true,
        },
        getForbiddenMoving = {
            type = "function",
            parameters = {},
            ["return"] = {
                "value",
                value = "nil",
            },
            Pure = true,
        },
        setForbiddenMoving = {
            type = "function",
            parameters = {
                "value",
                value = "bool",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        Meta = {
            GeneralDataVars = {
                { "ID", "Player" },
            },
        },
    },
}

return _METADATA
