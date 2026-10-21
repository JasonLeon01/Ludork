using System;

namespace Ludork.Services;

public sealed class BlueprintNodeSearchValidationEventArgs(
    LuaMetadataService metadata,
    BlueprintClassResolver classes) : EventArgs
{
    public LuaMetadataService Metadata { get; } = metadata;
    public BlueprintClassResolver Classes { get; } = classes;
}
