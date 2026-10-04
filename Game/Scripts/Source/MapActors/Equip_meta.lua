local _METADATA = {
    Equip = {
        moduleReturn = true,
        attrs = {
            "ID",
            "getSE"
        },
        bases = {
            { "Source.MapActors.ConditionalActor", "ConditionalActor" }
        },
        ID = {
            type = { enum = "Enums.GeneralData.Equip", valueType = "string" },
            default = "FILL_IT_BY_YOURSELF"
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
