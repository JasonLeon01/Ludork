local _METADATA = {
    Transition = {
        attrs = {},
        FreezeTransitionBackground = {
            type = "function",
            parameters = {},
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = true,
            LatentStates = {
                "Frozen",
                Frozen = {
                    true
                }
            }
        },
        RequestTransition = {
            type = "function",
            parameters = {
                "transitionName",
                "transitionTime",
                transitionName = "string",
                transitionTime = "float"
            },
            default = {
                [1] = "",
                [2] = 1.0
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = true,
            LatentStates = {
                "Finished",
                Finished = {
                    true
                }
            },
            Meta = {
                PathVars = {
                    {
                        "transitionName",
                        "/Game/Assets/Transitions"
                    }
                }
            }
        }
    }
}

return _METADATA
