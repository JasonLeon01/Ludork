local _METADATA = {
    ChildActorComponent = {
        attrs = {
            "className",
            "relativePosition",
        },
        bases = {
            { "Engine", "Component" },
        },
        className = {
            type = "string",
            default = "",
        },
        relativePosition = {
            type = "sf.Vector2f",
            default = { 0.0, 0.0 },
        },
    },
}

return _METADATA
