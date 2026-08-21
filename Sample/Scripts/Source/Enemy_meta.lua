local _METADATA = {
    Enemy = {
        attrs = {
            "ID",
            "infoComp",
            "childActorComp",
            "tickable",
            "collisionEnabled",
            "animatable",
            "animateWithoutMoving",
            "afterBattleVarChanges",
        },
        bases = {
            { "Engine", "Actor" },
            { "Source.Infos.EnemyInfo", "EnemyInfo" },
            { "Source.Battler", "Battler" },
        },
        ID = {
            type = "string",
            default = "FILL_IT_BY_YOURSELF",
            Meta = {
                GeneralDataVars = "Enemy",
            },
        },
        infoComp = {
            type = { "Source.Components.EnemyInfoComponent", "EnemyInfoComponent" },
            component = true,
            default = {
                MAXHP = 1000,
                ATK = 10,
                DEF = 10,
                EXP = 0,
                GOLD = 0,
                ANIMATION_KEY = "",
                name = "",
                desc = "",
                special = {},
                drops = {},
            },
        },
        childActorComp = {
            type = {
                "Source.Components.ChildActorComponent",
                "ChildActorComponent",
            },
            component = true,
            default = {
                className = "Source.EnemyDamageText.EnemyDamageText",
                relativePosition = { 0.0, 0.0 },
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
        animateWithoutMoving = {
            type = "bool",
            default = true,
        },
        afterBattleVarChanges = {
            type = "Dict[string, Tuple[string, any]]",
            default = {},
            Meta = {
                DictKeyMeta = {
                    InstVar = {
                        types = {
                            "int",
                            "float",
                        },
                    },
                },
                ItemMeta = {
                    TupleMeta = {
                        [1] = {
                            DropBox = {
                                "=",
                                "+",
                                "-",
                                "*",
                                "/",
                                "//",
                                "%",
                                "**",
                            },
                        },
                        [2] = {
                            InstVarValue = "$dictKey",
                        },
                    },
                },
            },
        },
        battle = {
            type = "function",
            parameters = {},
            ["return"] = {
                "return",
                ["return"] = "int",
            },
            ExecSplit = {
                "Win",
                "Lose",
                "Escape",
                Win = { 0 },
                Lose = { 1 },
                Escape = { 2 },
            },
        },
        onCollision = {
            type = "event",
            parameters = {
                "other",
                other = { "Engine", "Actor[]" },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        onDefeat = {
            type = "event",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        Meta = {
            GeneralDataVars = {
                { "ID", "Enemy" },
            },
        },
    },
}

return _METADATA
