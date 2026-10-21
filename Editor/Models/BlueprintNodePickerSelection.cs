namespace Ludork.Models;

public sealed record BlueprintNodePickerSelection(
    BlueprintGraphNodeDefinition Definition,
    int? TargetPinIndex);
