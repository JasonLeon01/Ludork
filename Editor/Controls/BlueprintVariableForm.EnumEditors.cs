using Avalonia.Controls;
using Ludork.Models;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private LuaEnumDefinition readEnum(LuaMetadataType type)
    {
        return (EnumService ?? throw new InvalidOperationException("An enum service is required for enum fields.")).Read(type);
    }

    private Control createEnumEditor(
        LuaMetadataType type,
        JsonNode? value,
        Action<JsonNode?, bool> changed)
    {
        return new EnumValueEditor(() => readEnum(type), value, next => changed(next, false), allowEmptyString: true);
    }
}
