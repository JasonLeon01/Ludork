local _METADATA = {
    EnemyDamageText = {
        attrs = {
            "tickable",
            "collisionEnabled",
            "requiredItemID",
            "textConfig",
            "damageTextOffset",
        },
        bases = {
            { "Engine", "Actor" },
        },
        tickable = {
            type = "bool",
            default = true,
        },
        collisionEnabled = {
            type = "bool",
            default = false,
        },
        requiredItemID = {
            type = "string",
            default = "EnemyBook",
            Meta = {
                GeneralDataVars = "Item",
            },
        },
        textConfig = {
            type = "string",
            default = "Enemy/DamageReadout",
        },
        damageTextOffset = {
            type = "sf.Vector2f",
            default = { 0.0, 0.0 },
        },
        onTick = {
            type = "event",
            parameters = {
                "deltaTime",
                deltaTime = "float",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        Meta = {
            GeneralDataVars = {
                { "requiredItemID", "Item" },
            },
        },
    },
}

return _METADATA
