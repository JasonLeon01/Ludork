using System.Collections.Generic;

namespace Ludork.Plugins.OfficialMotaMapFill.Runtime;

internal enum MotaActorCategory
{
    Unsupported,
    Door,
    Enemy,
    Item,
    Teleporter,
}

internal sealed record MotaPlayerInfo(
    string Key,
    string Name,
    double Hp,
    double Attack,
    double Defense);

internal sealed record MotaBlueprintInfo(
    string Reference,
    MotaActorCategory Category,
    string DoorKeyId,
    int DoorKeyCount,
    double? Damage,
    int? BattleResultCode,
    bool CanWin);

internal sealed record MotaRuntimeResult(
    bool Success,
    string Error,
    IReadOnlyList<MotaPlayerInfo> Players,
    IReadOnlyList<MotaBlueprintInfo> Blueprints,
    string SelectedPlayerKey)
{
    public static MotaRuntimeResult Failed(string error) =>
        new(false, error, [], [], string.Empty);
}
