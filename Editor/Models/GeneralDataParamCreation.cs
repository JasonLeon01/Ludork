namespace Ludork.Models;

public sealed record GeneralDataParamCreation(
    string Name,
    string Type,
    string? ItemType,
    string? ValueType,
    string DefaultText,
    string Comment);
