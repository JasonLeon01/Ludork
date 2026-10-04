local _METADATA = {
    Item = {
        moduleReturn = true,
        attrs = {
            "ID",
            "count",
            "getSE"
        },
        bases = {
            { "Source.MapActors.ConditionalActor", "ConditionalActor" }
        },
        ID = {
            type = { enum = "Enums.GeneralData.Item", valueType = "string" },
            default = ""
        },
        count = {
            type = "int",
            default = 1
        },
        getSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "/Game/Assets/Sounds",
                ConfigVars = { "Audio", "getSE" }
            }
        },
        Meta = {

            PathVars = {
                { "getSE", "/Game/Assets/Sounds" }
            },
            ConfigVars = {
                { "getSE", "Audio", "getSE" }
            }
        }
    }
}

return _METADATA
