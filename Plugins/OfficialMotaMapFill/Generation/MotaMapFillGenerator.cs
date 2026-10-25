using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialMotaMapFill.Generation;

internal static class MotaMapFillGenerator
{
    public static MotaMapFillResult Generate(MotaMapFillRequest request, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(request);
        cancellationToken.ThrowIfCancellationRequested();
        string? error = Validate(request);
        if (error is not null)
        {
            return new MotaMapFillResult(false, error, [], 0, 0, 0, request.Seed, []);
        }
        Generation generation = new Generation(request, cancellationToken);
        return generation.Run();
    }

    private static string? Validate(MotaMapFillRequest request)
    {
        int width = request.Walkable.GetLength(1);
        int height = request.Walkable.GetLength(0);
        if (width == 0 || height == 0 || request.Connections.GetLength(1) != width
            || request.Connections.GetLength(0) != height)
        {
            return "fill.invalidTerrain";
        }
        MotaRegion region = request.Region;
        if (region.X < 0 || region.Y < 0 || region.Width <= 0 || region.Height <= 0
            || (long)region.X + region.Width > width || (long)region.Y + region.Height > height)
        {
            return "fill.invalidRegion";
        }
        if (!double.IsFinite(request.PlayerHp) || request.PlayerHp <= 0)
        {
            return "fill.invalidPlayer";
        }
        if (request.Candidates.Any(candidate => string.IsNullOrWhiteSpace(candidate.BlueprintPath)
            || !Enum.IsDefined(candidate.Kind)
            || (candidate.Kind == MotaCandidateKind.Enemy
                && (!Enum.IsDefined(candidate.BattleCode) || !double.IsFinite(candidate.Damage) || candidate.Damage < 0)))
            || request.Candidates.GroupBy(candidate => candidate.BlueprintPath, StringComparer.Ordinal)
                .Any(group => group.Distinct().Count() > 1))
        {
            return "fill.invalidCandidates";
        }
        return null;
    }

    private sealed class Generation
    {
        private readonly MotaMapFillRequest request;
        private readonly CancellationToken cancellationToken;
        private readonly MotaMapTopology topology;
        private readonly StableRandom random;
        private readonly List<MotaPlacement> placements = [];
        private readonly HashSet<MotaCell> occupied;
        private readonly HashSet<MotaCell> items;
        private readonly HashSet<MotaCell> guards = [];
        private readonly HashSet<MotaCell> enemyCells = [];
        private readonly HashSet<MotaCell> entrancesWithRoomEnemies = [];
        private readonly HashSet<MotaCell> rewardReservations = [];
        private readonly HashSet<int> guardedCorridors = [];
        private readonly HashSet<int> protectedRooms = [];
        private readonly HashSet<int> roomsWithEnemies = [];
        private readonly HashSet<MotaCell> protectedPockets = [];
        private readonly HashSet<MotaCell> pocketsWithEnemies = [];
        private readonly HashSet<string> skipped = new(StringComparer.Ordinal);
        private readonly Dictionary<int, int> roomParities = [];
        private readonly Dictionary<MotaCell, int> pocketParities = [];
        private readonly Dictionary<MotaCell, List<int>> corridorsByCell = [];
        private readonly HashSet<MotaCell> pocketCells;
        private readonly Dictionary<int, MotaCell> roomRewards = [];
        private readonly Dictionary<MotaCell, MotaCell> pocketRewards = [];
        private readonly List<MotaActorCandidate> yellowDoors;
        private readonly List<MotaActorCandidate> blueDoors;
        private readonly List<MotaActorCandidate> lowEnemies;
        private readonly List<MotaActorCandidate> highEnemies;
        private readonly List<MotaActorCandidate> highRiskEnemies;
        private readonly List<MotaActorCandidate> itemCandidates;
        private readonly List<MotaMapTopology.Room> activeRooms;
        private readonly List<MotaMapTopology.Pocket> activePockets;
        private int highRiskEnemyCount;

        public Generation(MotaMapFillRequest request, CancellationToken cancellationToken)
        {
            this.request = request;
            this.cancellationToken = cancellationToken;
            topology = new MotaMapTopology(request, cancellationToken);
            pocketCells = topology.Pockets.SelectMany(pocket => pocket.Cells).ToHashSet();
            foreach (MotaMapTopology.Corridor corridor in topology.Corridors)
                foreach (MotaCell cell in corridor.Cells)
                {
                    if (!corridorsByCell.TryGetValue(cell, out List<int>? ids))
                    {
                        ids = [];
                        corridorsByCell.Add(cell, ids);
                    }
                    ids.Add(corridor.Id);
                }
            random = new StableRandom(request.Seed);
            occupied = new HashSet<MotaCell>(request.ReservedCells);
            occupied.UnionWith(request.TeleportCells);
            items = new HashSet<MotaCell>(request.ExistingItemCells);
            occupied.UnionWith(items);
            List<MotaActorCandidate> candidates = request.Candidates
                .OrderBy(candidate => candidate.BlueprintPath, StringComparer.Ordinal)
                .DistinctBy(candidate => candidate.BlueprintPath, StringComparer.Ordinal).ToList();
            yellowDoors = candidates.Where(candidate => candidate.Kind == MotaCandidateKind.YellowDoor).ToList();
            blueDoors = candidates.Where(candidate => candidate.Kind == MotaCandidateKind.BlueDoor).ToList();
            itemCandidates = candidates.Where(candidate => candidate.Kind == MotaCandidateKind.Item).ToList();
            List<MotaActorCandidate> enemies = candidates.Where(candidate => candidate.Kind == MotaCandidateKind.Enemy)
                .OrderBy(candidate => candidate.Damage).ThenBy(candidate => candidate.BlueprintPath, StringComparer.Ordinal).ToList();
            highRiskEnemies = enemies.Where(IsHighRisk).ToList();
            List<MotaActorCandidate> ordinary = enemies.Where(candidate => !IsHighRisk(candidate)).ToList();
            int lowCount = (ordinary.Count + 1) / 2;
            lowEnemies = ordinary.Take(lowCount).ToList();
            highEnemies = ordinary.Skip(lowCount).ToList();
            if (highEnemies.Count == 0)
                highEnemies.AddRange(ordinary);
            activeRooms = topology.Rooms.Where(room => room.Cells.Any(request.Region.Contains)).ToList();
            activePockets = topology.Pockets.Where(pocket => pocket.Cells.Any(request.Region.Contains)).ToList();
        }

        public MotaMapFillResult Run()
        {
            if (activeRooms.Count == 0 && activePockets.Count == 0)
                skipped.Add("fill.noRooms");
            ReserveRewards();
            MotaMapTopology.Room? highRiskRoom = SelectHighRiskRoom();
            List<MotaMapTopology.Room> orderedRooms = activeRooms.ToList();
            random.Shuffle(orderedRooms);
            foreach (MotaMapTopology.Room room in orderedRooms.OrderByDescending(room => room == highRiskRoom)
                .ThenByDescending(room => room.Cells.Count))
            {
                cancellationToken.ThrowIfCancellationRequested();
                GuardRoom(room, room == highRiskRoom);
            }
            foreach (MotaMapTopology.Pocket pocket in activePockets)
            {
                cancellationToken.ThrowIfCancellationRequested();
                GuardPocket(pocket);
            }
            FillRoomItems();
            FillPocketItems();
            FillCorridors();
            CheckCoverage();
            if (placements.Count == 0)
                skipped.Add("fill.noPlacements");
            return new MotaMapFillResult(placements.Count > 0,
                placements.Count > 0 ? string.Empty : "fill.noPlacements", placements.ToArray(),
                activeRooms.Count + activePockets.Count, activeRooms.Count(room => room.Cells.Count >= 9),
                highRiskEnemyCount, request.Seed, skipped.Order(StringComparer.Ordinal).ToArray());
        }

        private bool IsHighRisk(MotaActorCandidate candidate) =>
            candidate.BattleCode != MotaBattleCode.Win || candidate.Damage > request.PlayerHp / 2;

        private void ReserveRewards()
        {
            foreach (MotaMapTopology.Room room in activeRooms)
            {
                cancellationToken.ThrowIfCancellationRequested();
                int parity = random.Next(2);
                roomParities[room.Id] = parity;
                if (itemCandidates.Count > 0 && !room.Cells.Any(items.Contains)
                    && ReserveReward(room.Cells, parity) is MotaCell reward)
                    roomRewards[room.Id] = reward;
            }
            foreach (MotaMapTopology.Pocket pocket in activePockets)
            {
                int parity = random.Next(2);
                pocketParities[pocket.Gate] = parity;
                if (itemCandidates.Count > 0 && !pocket.Cells.Any(items.Contains)
                    && ReserveReward(pocket.Cells, parity, pocket.Gate) is MotaCell reward)
                    pocketRewards[pocket.Gate] = reward;
            }
        }

        private MotaCell? ReserveReward(IEnumerable<MotaCell> cells, int parity, MotaCell? gate = null)
        {
            HashSet<MotaCell> interior = cells.ToHashSet();
            List<MotaCell> available = cells.Where(cell => CanPlace(cell) && CanPlaceItem(cell)).ToList();
            random.Shuffle(available);
            if (available.Count == 0)
                return null;
            Dictionary<MotaCell, int> distances = [];
            if (gate is MotaCell entrance)
            {
                Queue<MotaCell> pending = new();
                distances[entrance] = 0;
                pending.Enqueue(entrance);
                while (pending.TryDequeue(out MotaCell cell))
                    foreach (MotaCell neighbor in topology.Neighbors(cell))
                        if (interior.Contains(neighbor) && distances.TryAdd(neighbor, distances[cell] + 1))
                            pending.Enqueue(neighbor);
            }
            MotaCell reward = available.OrderByDescending(cell => distances.GetValueOrDefault(cell))
                .ThenBy(cell => ((cell.X + cell.Y) & 1) == parity ? 0 : 1).First();
            rewardReservations.Add(reward);
            return reward;
        }

        private MotaMapTopology.Room? SelectHighRiskRoom()
        {
            if (highRiskEnemies.Count == 0)
                return null;
            List<MotaMapTopology.Room> completeRooms = activeRooms
                .Where(room => room.Cells.Count >= 9 && room.Cells.All(request.Region.Contains)).ToList();
            if (completeRooms.Count == 0)
            {
                skipped.Add("fill.noHighRiskRoom");
                return null;
            }
            int maximum = completeRooms.Max(room => room.Cells.Count);
            List<MotaMapTopology.Room> candidates = completeRooms
                .Where(room => room.Cells.Count == maximum && GetEntrances(room, true).Count > 0).ToList();
            if (candidates.Count == 0)
            {
                skipped.Add("fill.noHighRiskEntrance");
                return null;
            }
            return candidates[random.Next(candidates.Count)];
        }

        private void GuardRoom(MotaMapTopology.Room room, bool highRisk)
        {
            bool large = room.Cells.Count >= 9;
            List<MotaActorCandidate> enemies = highRisk ? highRiskEnemies : large ? highEnemies : lowEnemies;
            if (large && blueDoors.Count == 0)
                skipped.Add("fill.noBlueDoors");
            if (enemies.Count == 0)
                skipped.Add("fill.noOrdinaryEnemies");
            List<GuardPosition> entrances = GetEntrances(room, highRisk);
            random.Shuffle(entrances);
            GuardPosition? entrance = entrances.OrderBy(position => position.Distance)
                .ThenByDescending(position => topology.IsStraightGate(position.Gate)).FirstOrDefault();
            if (!highRisk && entrances.Any(candidate => enemyCells.Contains(candidate.Gate)))
            {
                protectedRooms.Add(room.Id);
                roomsWithEnemies.Add(room.Id);
            }
            MotaCell? inner = entrance?.Inner ?? FindInnerGuard(room, room.Entrances.Select(candidate => candidate.InnerCell));
            if (!roomsWithEnemies.Contains(room.Id) && enemies.Count > 0 && inner is MotaCell cell && (!highRisk || entrance is not null)
                && AddGuard(cell, Pick(enemies)))
            {
                protectedRooms.Add(room.Id);
                roomsWithEnemies.Add(room.Id);
                if (highRisk)
                    highRiskEnemyCount++;
            }
            if (roomsWithEnemies.Contains(room.Id))
                foreach (MotaMapTopology.Entrance entry in room.Entrances)
                {
                    MotaMapTopology.Corridor corridor = topology.Corridors[entry.CorridorId];
                    MotaCell farEnd = entry.Reverse ? corridor.Cells[0] : corridor.Cells[^1];
                    if (!pocketCells.Contains(farEnd))
                        entrancesWithRoomEnemies.Add(entry.Reverse ? corridor.Cells[^1] : corridor.Cells[0]);
                }
            if (entrance is not null)
            {
                List<MotaActorCandidate> doors = large ? blueDoors : yellowDoors;
                if (guards.Contains(entrance.Gate)
                    || doors.Count > 0 && AddGuard(entrance.Gate, Pick(doors))
                    || !large && !roomsWithEnemies.Contains(room.Id) && AddSmallGuard(entrance.Gate))
                    protectedRooms.Add(room.Id);
            }
            if (!protectedRooms.Contains(room.Id))
                skipped.Add("fill.noEntrance");
        }

        private List<GuardPosition> GetEntrances(MotaMapTopology.Room room, bool requireInner)
        {
            List<GuardPosition> result = [];
            foreach (MotaMapTopology.Entrance entrance in room.Entrances)
            {
                cancellationToken.ThrowIfCancellationRequested();
                MotaCell? inner = FindInnerGuard(room, [entrance.InnerCell]);
                if (requireInner && inner is null)
                    continue;
                MotaMapTopology.Corridor corridor = topology.Corridors[entrance.CorridorId];
                MotaCell farEnd = entrance.Reverse ? corridor.Cells[0] : corridor.Cells[^1];
                MotaCell[] farNeighbors = topology.Neighbors(farEnd).ToArray();
                bool leadsOutside = farNeighbors.Any(cell => topology.RoomId(cell) >= 0 && topology.RoomId(cell) != room.Id)
                    || farNeighbors.Length >= 3 || request.TeleportCells.Contains(farEnd);
                if (!leadsOutside || pocketCells.Contains(farEnd))
                    continue;
                List<GuardPosition> gates = [];
                for (int distance = 0; distance < corridor.Cells.Count; distance++)
                {
                    MotaCell gate = corridor.Cells[entrance.Reverse ? corridor.Cells.Count - distance - 1 : distance];
                    if (topology.IsGate(gate) && (CanPlaceGuard(gate) || guards.Contains(gate)))
                        gates.Add(new GuardPosition(gate, inner, distance));
                }
                GuardPosition? position = gates.OrderByDescending(candidate => guards.Contains(candidate.Gate))
                    .ThenBy(candidate => candidate.Distance).FirstOrDefault();
                if (position is not null)
                    result.Add(position);
            }
            return result.DistinctBy(position => (position.Gate, position.Inner)).ToList();
        }

        private MotaCell? FindInnerGuard(MotaMapTopology.Room room, IEnumerable<MotaCell> entryCells)
        {
            MotaCell[] entries = entryCells.ToArray();
            List<MotaCell> available = room.Cells.Where(CanPlaceGuard).ToList();
            if (available.Count == 0)
                return null;
            return available.OrderBy(cell => entries.Length == 0
                    ? topology.Neighbors(cell).Count()
                    : entries.Min(entry => Math.Abs(entry.X - cell.X) + Math.Abs(entry.Y - cell.Y)))
                .ThenBy(cell => cell.Y).ThenBy(cell => cell.X).First();
        }

        private void GuardPocket(MotaMapTopology.Pocket pocket)
        {
            List<MotaCell> available = pocket.Cells.Where(CanPlaceGuard)
                .OrderBy(cell => Math.Abs(cell.X - pocket.Gate.X) + Math.Abs(cell.Y - pocket.Gate.Y)).ToList();
            if (lowEnemies.Count > 0 && available.Count > 0 && AddGuard(available[0], Pick(lowEnemies)))
            {
                protectedPockets.Add(pocket.Gate);
                pocketsWithEnemies.Add(pocket.Gate);
                entrancesWithRoomEnemies.Add(pocket.Gate);
            }
            if (guards.Contains(pocket.Gate)
                || yellowDoors.Count > 0 && AddGuard(pocket.Gate, Pick(yellowDoors))
                || !pocketsWithEnemies.Contains(pocket.Gate) && AddSmallGuard(pocket.Gate))
                protectedPockets.Add(pocket.Gate);
        }

        private void FillCorridors()
        {
            List<MotaMapTopology.Corridor> corridors = topology.Corridors.ToList();
            random.Shuffle(corridors);
            foreach (MotaMapTopology.Corridor corridor in corridors)
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (guardedCorridors.Contains(corridor.Id))
                    continue;
                List<MotaCell> gates = corridor.Cells.Where(cell => topology.IsGate(cell) && CanPlaceGuard(cell)
                    && !pocketCells.Contains(cell)
                    && (!entrancesWithRoomEnemies.Contains(cell) || yellowDoors.Count > 0)).ToList();
                if (gates.Count == 0)
                    continue;
                int center = corridor.Cells.Count / 2;
                random.Shuffle(gates);
                MotaCell gate = gates.OrderByDescending(topology.IsStraightGate)
                    .ThenBy(cell => Math.Abs(corridor.Cells.IndexOf(cell) - center)).First();
                AddSmallGuard(gate);
            }
        }

        private bool AddSmallGuard(MotaCell cell)
        {
            if (!CanPlaceGuard(cell))
                return false;
            if (yellowDoors.Count == 0 && lowEnemies.Count == 0)
            {
                skipped.Add("fill.noSmallGuards");
                return false;
            }
            bool canUseEnemy = lowEnemies.Count > 0 && !entrancesWithRoomEnemies.Contains(cell);
            if (yellowDoors.Count == 0 && !canUseEnemy)
                return false;
            bool useDoor = yellowDoors.Count > 0 && (!canUseEnemy || random.Next(2) == 0);
            return AddGuard(cell, Pick(useDoor ? yellowDoors : lowEnemies));
        }

        private bool AddGuard(MotaCell cell, MotaActorCandidate candidate)
        {
            if (!CanPlaceGuard(cell))
                return false;
            Add(cell, candidate);
            guards.Add(cell);
            if (candidate.Kind == MotaCandidateKind.Enemy)
                enemyCells.Add(cell);
            if (corridorsByCell.TryGetValue(cell, out List<int>? ids))
                guardedCorridors.UnionWith(ids);
            return true;
        }

        private void FillRoomItems()
        {
            if (itemCandidates.Count == 0)
            {
                skipped.Add("fill.noItems");
                return;
            }
            foreach (MotaMapTopology.Room room in activeRooms)
            {
                MotaCell? reward = roomRewards.TryGetValue(room.Id, out MotaCell cell) ? cell : null;
                FillRewards(room.Cells, roomParities[room.Id], reward);
            }
        }

        private void FillPocketItems()
        {
            if (itemCandidates.Count == 0)
                return;
            foreach (MotaMapTopology.Pocket pocket in activePockets)
            {
                MotaCell? reward = pocketRewards.TryGetValue(pocket.Gate, out MotaCell cell) ? cell : null;
                FillRewards(pocket.Cells, pocketParities[pocket.Gate], reward);
            }
        }

        private void FillRewards(IReadOnlyList<MotaCell> interior, int parity, MotaCell? reserved)
        {
            cancellationToken.ThrowIfCancellationRequested();
            int target = (int)Math.Ceiling(interior.Count(CanPlace) * 0.4);
            int placed = 0;
            if (reserved is MotaCell reward && CanPlace(reward))
            {
                Add(reward, Pick(itemCandidates));
                placed++;
            }
            List<MotaCell> cells = interior.Where(CanPlace).ToList();
            random.Shuffle(cells);
            foreach (MotaCell cell in cells.OrderBy(cell => ((cell.X + cell.Y) & 1) == parity ? 0 : 1))
            {
                if (placed >= target)
                    break;
                if (!CanPlaceItem(cell))
                    continue;
                Add(cell, Pick(itemCandidates));
                placed++;
            }
            if (placed < target)
                skipped.Add("fill.itemSpacingLimited");
        }

        private void CheckCoverage()
        {
            if (activeRooms.Any(room => !roomsWithEnemies.Contains(room.Id))
                || activePockets.Any(pocket => pocket.Cells.Count > 1 && !pocketsWithEnemies.Contains(pocket.Gate)))
                skipped.Add("fill.roomsWithoutEnemies");
            if (activeRooms.Any(room => room.Cells.Any(request.Region.Contains) && !protectedRooms.Contains(room.Id))
                || activePockets.Any(pocket => !protectedPockets.Contains(pocket.Gate)))
                skipped.Add("fill.roomsWithoutGuards");
            if (itemCandidates.Count > 0 && (activeRooms.Any(room => !room.Cells.Any(items.Contains))
                || activePockets.Any(pocket => !pocket.Cells.Any(items.Contains))))
                skipped.Add("fill.roomsWithoutItems");
        }

        private bool CanPlaceItem(MotaCell cell) => !CompletesLine(cell, 1, 0) && !CompletesLine(cell, 0, 1);

        private bool CompletesLine(MotaCell cell, int dx, int dy)
        {
            for (int start = -2; start <= 0; start++)
            {
                bool complete = true;
                for (int offset = start; offset < start + 3; offset++)
                {
                    MotaCell neighbor = new(cell.X + dx * offset, cell.Y + dy * offset);
                    if (offset != 0 && !items.Contains(neighbor) && !rewardReservations.Contains(neighbor))
                    {
                        complete = false;
                        break;
                    }
                }
                if (complete)
                    return true;
            }
            return false;
        }

        private bool CanPlace(MotaCell cell) => request.Region.Contains(cell) && topology.IsWalkable(cell) && !occupied.Contains(cell);
        private bool CanPlaceGuard(MotaCell cell) => CanPlace(cell) && !rewardReservations.Contains(cell);
        private MotaActorCandidate Pick(List<MotaActorCandidate> candidates) => candidates[random.Next(candidates.Count)];

        private void Add(MotaCell cell, MotaActorCandidate candidate)
        {
            occupied.Add(cell);
            placements.Add(new MotaPlacement(cell, candidate.BlueprintPath, candidate.Kind));
            if (candidate.Kind == MotaCandidateKind.Item)
                items.Add(cell);
        }

        private sealed record GuardPosition(MotaCell Gate, MotaCell? Inner, int Distance);
    }
}
