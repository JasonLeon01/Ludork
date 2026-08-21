local _METADATA = {
    EnemyInfoComponent = {
        attrs = {
            "name",
            "desc",
            "special",
            "drops",
        },
        bases = {
            { "Source.Components.BattlerInfoComponent", "BattlerInfoComponent" },
        },
        name = {
            type = "string",
            default = "",
        },
        desc = {
            type = "string",
            default = "",
        },
        special = {
            type = "Dict[string, any]",
            default = {},
        },
        drops = {
            type = "Dict[string, sf.Vector2i]",
            default = {},
        },
    },
}

return _METADATA
