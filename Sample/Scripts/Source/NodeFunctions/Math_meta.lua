local _METADATA = {
    Math = {
        attrs = {},
        BuildVector2f = {
            type = "function",
            parameters = {
                "x",
                "y",
                x = "float",
                y = "float",
            },
            default = {
                [1] = 0.0,
                [2] = 0.0,
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        BuildVector2i = {
            type = "function",
            parameters = {
                "x",
                "y",
                x = "int",
                y = "int",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "sf.Vector2i",
            },
            Pure = true,
        },
        BuildVector2u = {
            type = "function",
            parameters = {
                "x",
                "y",
                x = "int",
                y = "int",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "sf.Vector2u",
            },
            Pure = true,
        },
        BuildVector3f = {
            type = "function",
            parameters = {
                "x",
                "y",
                "z",
                x = "float",
                y = "float",
                z = "float",
            },
            default = {
                [1] = 0.0,
                [2] = 0.0,
                [3] = 0.0,
            },
            ["return"] = {
                "value",
                value = "sf.Vector3f",
            },
            Pure = true,
        },
        BuildVector3i = {
            type = "function",
            parameters = {
                "x",
                "y",
                "z",
                x = "int",
                y = "int",
                z = "int",
            },
            default = {
                [1] = 0,
                [2] = 0,
                [3] = 0,
            },
            ["return"] = {
                "value",
                value = "sf.Vector3i",
            },
            Pure = true,
        },
        IsNearZero = {
            type = "function",
            parameters = {
                "num",
                "epsilon",
                num = "float",
                epsilon = "float",
            },
            default = {
                [2] = 0.1,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        IsVector2NearZero = {
            type = "function",
            parameters = {
                "v",
                "epsilon",
                v = "sf.Vector2f",
                epsilon = "float",
            },
            default = {
                [2] = 0.1,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        IsVector3NearZero = {
            type = "function",
            parameters = {
                "v",
                "epsilon",
                v = "sf.Vector3f",
                epsilon = "float",
            },
            default = {
                [2] = 0.1,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        Vector2fRound = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector2fFloor = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector2fCeil = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        ToVector2f = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2i",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        ToVector2i = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2i",
            },
            Pure = true,
        },
        ToVector2u = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2u",
            },
            Pure = true,
        },
        ToVector3f = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector3i",
            },
            ["return"] = {
                "value",
                value = "sf.Vector3f",
            },
            Pure = true,
        },
        ToVector3i = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector3i",
            },
            Pure = true,
        },
        ToIntRect = {
            type = "function",
            parameters = {
                "x",
                "y",
                "width",
                "height",
                x = "int",
                y = "int",
                width = "int",
                height = "int",
            },
            default = {
                [1] = 0,
                [2] = 0,
                [3] = 32,
                [4] = 32,
            },
            ["return"] = {
                "value",
                value = "sf.IntRect",
            },
            Pure = true,
        },
        ToFloatRect = {
            type = "function",
            parameters = {
                "x",
                "y",
                "width",
                "height",
                x = "float",
                y = "float",
                width = "float",
                height = "float",
            },
            default = {
                [1] = 0.0,
                [2] = 0.0,
                [3] = 32.0,
                [4] = 32.0,
            },
            ["return"] = {
                "value",
                value = "sf.FloatRect",
            },
            Pure = true,
        },
        Clamp = {
            type = "function",
            parameters = {
                "value",
                "min_val",
                "max_val",
                value = "nil",
                min_val = "nil",
                max_val = "nil",
            },
            default = {
                [1] = 0.0,
                [2] = 0.0,
                [3] = 1.0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Lerp = {
            type = "function",
            parameters = {
                "a",
                "b",
                "t",
                a = "float",
                b = "float",
                t = "float",
            },
            default = {
                [1] = 0.0,
                [2] = 1.0,
                [3] = 0.5,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Abs = {
            type = "function",
            parameters = {
                "value",
                value = "float",
            },
            default = {
                [1] = 0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        ToInt = {
            type = "function",
            parameters = {
                "value",
                value = "float",
            },
            default = {
                [1] = 0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        ToFloat = {
            type = "function",
            parameters = {
                "value",
                value = "float",
            },
            default = {
                [1] = 0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Max = {
            type = "function",
            parameters = {
                "values",
                values = "any[]",
            },
            default = {
                [1] = {},
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        Min = {
            type = "function",
            parameters = {
                "values",
                values = "any[]",
            },
            default = {
                [1] = {},
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
        },
        Sqrt = {
            type = "function",
            parameters = {
                "value",
                value = "float",
            },
            default = {
                [1] = 0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Pow = {
            type = "function",
            parameters = {
                "base",
                "exp",
                base = "float",
                exp = "float",
            },
            default = {
                [1] = 1,
                [2] = 2,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector2Distance = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector2f",
                v2 = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector3Distance = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector3f",
                v2 = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector2Dot = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector2f",
                v2 = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector3Dot = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector3f",
                v2 = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector2Cross = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector2f",
                v2 = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector3Cross = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector3f",
                v2 = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector3f",
            },
            Pure = true,
        },
        Vector2Length = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector3Length = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector2LengthSquared = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector3LengthSquared = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector2Normalized = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector3Normalized = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector3f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector3f",
            },
            Pure = true,
        },
        GetAngle = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Angle",
            },
            Pure = true,
        },
        GetAngleTo = {
            type = "function",
            parameters = {
                "v1",
                "v2",
                v1 = "sf.Vector2f",
                v2 = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Angle",
            },
            Pure = true,
        },
        AsDegrees = {
            type = "function",
            parameters = {
                "angle",
                angle = "sf.Angle",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        AsRadians = {
            type = "function",
            parameters = {
                "angle",
                angle = "sf.Angle",
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        Vector2ComponentWiseDiv = {
            type = "function",
            parameters = {
                "v",
                "div",
                v = "sf.Vector2f",
                div = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector2ComponentWiseMul = {
            type = "function",
            parameters = {
                "v",
                "mul",
                v = "sf.Vector2f",
                mul = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector2Perpendicular = {
            type = "function",
            parameters = {
                "v",
                v = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector2ProjectedOnto = {
            type = "function",
            parameters = {
                "v",
                "axis",
                v = "sf.Vector2f",
                axis = "sf.Vector2f",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        Vector2RotatedBy = {
            type = "function",
            parameters = {
                "v",
                "phi",
                v = "sf.Vector2f",
                phi = "sf.Angle",
            },
            ["return"] = {
                "value",
                value = "sf.Vector2f",
            },
            Pure = true,
        },
        DegreesToAngle = {
            type = "function",
            parameters = {
                "degrees_",
                degrees_ = "float",
            },
            default = {
                [1] = 0.0,
            },
            ["return"] = {
                "value",
                value = "sf.Angle",
            },
            Pure = true,
        },
        RadiansToAngle = {
            type = "function",
            parameters = {
                "radians_",
                radians_ = "float",
            },
            default = {
                [1] = 0.0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        RandomInt = {
            type = "function",
            parameters = {
                "min_val",
                "max_val",
                min_val = "int",
                max_val = "int",
            },
            default = {
                [1] = 0,
                [2] = 100,
            },
            ["return"] = {
                "value",
                value = "int",
            },
            Pure = true,
        },
        RandomFloat = {
            type = "function",
            parameters = {
                "min_val",
                "max_val",
                min_val = "float",
                max_val = "float",
            },
            default = {
                [1] = 0.0,
                [2] = 1.0,
            },
            ["return"] = {
                "value",
                value = "float",
            },
            Pure = true,
        },
        GCD = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "int",
                b = "int",
            },
            default = {
                [1] = 1,
                [2] = 1,
            },
            ["return"] = {
                "value",
                value = "int",
            },
            Pure = true,
        },
        LCM = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "int",
                b = "int",
            },
            default = {
                [1] = 1,
                [2] = 1,
            },
            ["return"] = {
                "value",
                value = "int",
            },
            Pure = true,
        },
        ADD = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
            Meta = {
                DisplayName = "+",
            },
        },
        SUB = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
            Meta = {
                DisplayName = "-",
            },
        },
        MUL = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 1,
                [2] = 1,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
            Meta = {
                DisplayName = "*",
            },
        },
        DIV = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 1,
                [2] = 1,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
            Meta = {
                DisplayName = "/",
            },
        },
        MOD = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 1,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
            Meta = {
                DisplayName = "%",
            },
        },
        POW = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 1,
                [2] = 1,
            },
            ["return"] = {
                "value",
                value = "any",
            },
            Pure = true,
            Meta = {
                DisplayName = "**",
            },
        },
        EQUALS = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                DisplayName = "==",
            },
        },
        NOT_EQUALS = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                DisplayName = "!=",
            },
        },
        LESS = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                DisplayName = "<",
            },
        },
        LESS_EQUALS = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                DisplayName = "<=",
            },
        },
        GREATER = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                DisplayName = ">",
            },
        },
        GREATER_EQUALS = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [1] = 0,
                [2] = 0,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
            Meta = {
                DisplayName = ">=",
            },
        },
        AND = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "bool",
                b = "bool",
            },
            default = {
                [1] = false,
                [2] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        OR = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "bool",
                b = "bool",
            },
            default = {
                [1] = false,
                [2] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        NOT = {
            type = "function",
            parameters = {
                "a",
                a = "bool",
            },
            default = {
                [1] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        XOR = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "bool",
                b = "bool",
            },
            default = {
                [1] = false,
                [2] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        NAND = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "bool",
                b = "bool",
            },
            default = {
                [1] = false,
                [2] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        NOR = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "bool",
                b = "bool",
            },
            default = {
                [1] = false,
                [2] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        XNOR = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "bool",
                b = "bool",
            },
            default = {
                [1] = false,
                [2] = false,
            },
            ["return"] = {
                "value",
                value = "bool",
            },
            Pure = true,
        },
        IADD = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
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
                DisplayName = "+=",
            },
        },
        ISUB = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
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
                DisplayName = "-=",
            },
        },
        IMUL = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [2] = 2,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DisplayName = "*=",
            },
        },
        IDIV = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [2] = 2,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DisplayName = "/=",
            },
        },
        IMOD = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [2] = 2,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DisplayName = "%=",
            },
        },
        IPOW = {
            type = "function",
            parameters = {
                "a",
                "b",
                a = "any",
                b = "any",
            },
            default = {
                [2] = 2,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DisplayName = "**=",
            },
        },
    },
}

return _METADATA
