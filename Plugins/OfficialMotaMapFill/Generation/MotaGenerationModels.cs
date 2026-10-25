using System;
using System.Collections.Generic;

namespace Ludork.Plugins.OfficialMotaMapFill.Generation;

internal readonly record struct MotaCell(int X, int Y);

internal readonly record struct MotaRegion(int X, int Y, int Width, int Height)
{
    public bool Contains(MotaCell cell)
    {
        return cell.X >= X && cell.Y >= Y && (long)cell.X < (long)X + Width && (long)cell.Y < (long)Y + Height;
    }
}

[Flags]
internal enum MotaConnections : byte
{
    None = 0,
    Up = 1,
    Right = 2,
    Down = 4,
    Left = 8,
    All = Up | Right | Down | Left,
}

internal enum MotaCandidateKind
{
    YellowDoor,
    BlueDoor,
    Enemy,
    Item,
}

internal enum MotaBattleCode
{
    Win = 1,
    CannotDamage = 2,
    Lethal = 3,
}

internal sealed record MotaActorCandidate(
    string BlueprintPath,
    MotaCandidateKind Kind,
    MotaBattleCode BattleCode = MotaBattleCode.Win,
    double Damage = 0);

internal sealed record MotaMapFillRequest(
    bool[,] Walkable,
    MotaConnections[,] Connections,
    MotaRegion Region,
    IReadOnlySet<MotaCell> ReservedCells,
    IReadOnlySet<MotaCell> ExistingItemCells,
    IReadOnlySet<MotaCell> TeleportCells,
    IReadOnlyList<MotaActorCandidate> Candidates,
    double PlayerHp,
    int Seed);

internal sealed record MotaPlacement(MotaCell Cell, string BlueprintPath, MotaCandidateKind Kind);

internal sealed record MotaMapFillResult(
    bool Succeeded,
    string ErrorKey,
    IReadOnlyList<MotaPlacement> Placements,
    int RoomCount,
    int LargeRoomCount,
    int HighRiskEnemyCount,
    int Seed,
    IReadOnlyList<string> SkippedReasons);
