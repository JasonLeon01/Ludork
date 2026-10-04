local _METADATA = {
    EnemyDamageText = {
        moduleReturn = true,
        attrs = {
            "tickable",
            "collisionEnabled",
            "requiredItemID",
            "textConfig",
            "damageTextOffset"
        },
        bases = {
            { "Engine", "Actor" }
        },
        tickable = {
            type = "bool",
            default = true
        },
        collisionEnabled = {
            type = "bool",
            default = false
        },
        requiredItemID = {
            type = { enum = "Enums.GeneralData.Item", valueType = "string" },
            default = "EnemyBook"
        },
        textConfig = {
            type = "string",
            default = "Enemy/DamageReadout"
        },
        damageTextOffset = {
            type = "sf.Vector2f",
            default = { 0.0, 0.0 }
        },
        onTick = {
            type = "event",
            parameters = {
                "deltaTime",
                deltaTime = "float"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        }
    }
}

return _METADATA
