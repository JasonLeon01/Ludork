using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public static partial class RandomMapGenerator
{
    private static bool RepairAndValidate(
        GenerationCandidate candidate,
        HashSet<RandomMapPoint> markers,
        RandomMapMode mode,
        int targetWalls,
        StableRandom random,
        out string failure)
    {
        failure = string.Empty;
        bool[,] walls = candidate.Walls;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int windowSize = mode == RandomMapMode.Lattice ? 4 : 8;

        SealVirtualBoundary(walls);
        foreach (RandomMapPoint marker in markers)
        {
            walls[marker.Y, marker.X] = false;
        }

        if (mode == RandomMapMode.Lattice && !ConnectEmptyComponents(walls))
        {
            failure = "noConnectedLayout";
            return false;
        }

        if (mode == RandomMapMode.Rooms
            && !RepairRoomOpenings(candidate, markers, random, out failure))
        {
            return false;
        }

        HashSet<RandomMapPoint> protectedWalls = CollectRoomRingWalls(candidate);
        protectedWalls.UnionWith(CollectVirtualBoundaryWalls(walls));
        if (!RepairWallBlocks(walls, protectedWalls, random))
        {
            failure = "noWallBlocks";
            return false;
        }

        if (!ConnectEmptyComponents(walls, protectedWalls))
        {
            failure = "noConnectedLayout";
            return false;
        }

        HashSet<RandomMapPoint> protectedEmpty = CollectProtectedEmptyCells(candidate, markers);
        if (!RepairEmptyWindows(walls, protectedEmpty, windowSize, random))
        {
            failure = "noUniformLayout";
            return false;
        }

        Func<bool>? styleValidator = null;
        if (mode == RandomMapMode.Lattice && HasValidLatticeStyle(candidate))
        {
            styleValidator = () => HasValidLatticeStyle(candidate);
        }
        else if (mode == RandomMapMode.Rooms
            && HasValidRoomStyle(candidate, markers, out _))
        {
            styleValidator =
                () => HasValidRoomStyle(candidate, markers, out _);
        }

        int wallCount = CountWalls(walls);
        if (wallCount < targetWalls)
        {
            IncreaseWalls(
                walls,
                targetWalls,
                protectedEmpty,
                protectedWalls,
                windowSize,
                styleValidator,
                random);
        }

        wallCount = CountWalls(walls);
        if (wallCount > targetWalls
            && !ReduceWalls(
                walls,
                targetWalls,
                windowSize,
                protectedWalls,
                random))
        {
            failure = "noSafeDensity";
            return false;
        }

        foreach (RandomMapPoint marker in markers)
        {
            if (walls[marker.Y, marker.X])
            {
                failure = "noValidTopology";
                return false;
            }
        }

        if (CountWalls(walls) > targetWalls)
        {
            failure = "noSafeDensity";
            return false;
        }

        if (!HasSingleEmptyComponent(walls))
        {
            failure = "noConnectedLayout";
            return false;
        }

        if (!HasSealedVirtualBoundary(walls))
        {
            failure = "noValidTopology";
            return false;
        }

        if (HasEmptyWindow(walls, windowSize))
        {
            failure = "noUniformLayout";
            return false;
        }

        if (HasWallBlock(walls))
        {
            failure = "noWallBlocks";
            return false;
        }

        bool validStyle;
        if (mode == RandomMapMode.Lattice)
        {
            validStyle = HasValidLatticeStyle(candidate);
        }
        else
        {
            validStyle = HasValidRoomStyle(candidate, markers, out failure);
        }

        if (!validStyle)
        {
            failure = mode == RandomMapMode.Lattice
                ? "latticeStyleFailed"
                : failure;
        }

        return validStyle;
    }

    private static HashSet<RandomMapPoint> CollectProtectedEmptyCells(
        GenerationCandidate candidate,
        HashSet<RandomMapPoint> markers)
    {
        HashSet<RandomMapPoint> protectedEmpty = [.. markers, .. candidate.MainPath];
        foreach (MapRect room in candidate.Rooms)
        {
            for (int y = room.Y; y < room.Bottom; y++)
            {
                for (int x = room.X; x < room.Right; x++)
                {
                    protectedEmpty.Add(new RandomMapPoint(x, y));
                }
            }
        }

        foreach (IReadOnlyList<RandomMapPoint> doors in candidate.RoomDoors.Values)
        {
            protectedEmpty.UnionWith(doors);
        }

        protectedEmpty.ExceptWith(candidate.RoomNotches);
        return protectedEmpty;
    }

    private static bool RepairWallBlocks(
        bool[,] walls,
        HashSet<RandomMapPoint> protectedWalls,
        StableRandom random)
    {
        int maximumPasses = Math.Max(4, GetPlayableArea(walls) / 4);

        for (int pass = 0; pass < maximumPasses; pass++)
        {
            int height = walls.GetLength(0);
            int width = walls.GetLength(1);
            Dictionary<RandomMapPoint, int> pressures = [];

            for (int y = 0; y < height - 1; y++)
            {
                for (int x = 0; x < width - 1; x++)
                {
                    if (!IsWallBlock(walls, x, y))
                    {
                        continue;
                    }

                    RandomMapPoint[] blockPoints =
                    [
                        new RandomMapPoint(x, y),
                        new RandomMapPoint(x + 1, y),
                        new RandomMapPoint(x, y + 1),
                        new RandomMapPoint(x + 1, y + 1),
                    ];
                    foreach (RandomMapPoint point in blockPoints)
                    {
                        if (!protectedWalls.Contains(point))
                        {
                            pressures[point] = pressures.GetValueOrDefault(point) + 1;
                        }
                    }
                }
            }

            if (!HasWallBlock(walls))
            {
                return true;
            }

            List<RandomMapPoint> candidates = pressures.Keys.ToList();
            if (candidates.Count == 0)
            {
                return false;
            }

            random.Shuffle(candidates);
            candidates.Sort(
                (left, right) => pressures[right].CompareTo(pressures[left]));
            int opened = 0;
            foreach (RandomMapPoint candidate in candidates)
            {
                if (walls[candidate.Y, candidate.X]
                    && CountWallBlocksContaining(
                        walls,
                        candidate.X,
                        candidate.Y) > 0)
                {
                    walls[candidate.Y, candidate.X] = false;
                    opened++;
                }
            }

            if (opened == 0)
            {
                return false;
            }
        }

        return !HasWallBlock(walls);
    }

    private static void IncreaseWalls(
        bool[,] walls,
        int targetWalls,
        HashSet<RandomMapPoint> protectedEmpty,
        HashSet<RandomMapPoint> protectedWalls,
        int windowSize,
        Func<bool>? additionalValidator,
        StableRandom random)
    {
        int wallCount = CountWalls(walls);
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        List<RandomMapPoint> addedWalls = [];

        while (wallCount < targetWalls)
        {
            List<RandomMapPoint> candidates = [];

            for (int y = 1; y < height - 1; y++)
            {
                for (int x = 1; x < width - 1; x++)
                {
                    RandomMapPoint point = new RandomMapPoint(x, y);
                    if (!walls[y, x]
                        && !protectedEmpty.Contains(point)
                        && !protectedWalls.Contains(point)
                        && CanAddWallAtEdge(walls, x, y)
                        && !WouldCreateWallBlock(walls, x, y))
                    {
                        candidates.Add(point);
                    }
                }
            }

            random.Shuffle(candidates);
            candidates = candidates
                .OrderBy(point => CountWallBlockThreats(
                    walls,
                    point.X,
                    point.Y))
                .ThenBy(point => CountOpenNeighbors(
                    walls,
                    point.X,
                    point.Y))
                .ToList();
            int added = 0;

            foreach (RandomMapPoint candidate in candidates)
            {
                if (wallCount >= targetWalls)
                {
                    break;
                }

                if (walls[candidate.Y, candidate.X]
                    || protectedEmpty.Contains(candidate)
                    || protectedWalls.Contains(candidate)
                    || !CanAddWallAtEdge(walls, candidate.X, candidate.Y)
                    || WouldCreateWallBlock(walls, candidate.X, candidate.Y))
                {
                    continue;
                }

                walls[candidate.Y, candidate.X] = true;
                if (HasSingleEmptyComponent(walls))
                {
                    wallCount++;
                    addedWalls.Add(candidate);
                    added++;
                    continue;
                }

                walls[candidate.Y, candidate.X] = false;
            }

            if (added == 0)
            {
                break;
            }
        }

        if (additionalValidator is not null)
        {
            for (int index = addedWalls.Count - 1;
                index >= 0
                    && (!additionalValidator()
                        || HasEmptyWindow(walls, windowSize));
                index--)
            {
                RandomMapPoint point = addedWalls[index];
                walls[point.Y, point.X] = false;
                wallCount--;
            }
        }

    }

    private static bool CanAddWallAtEdge(bool[,] walls, int x, int y)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        return IsInterior(x, y, width, height);
    }

    private static bool RepairRoomOpenings(
        GenerationCandidate candidate,
        HashSet<RandomMapPoint> markers,
        StableRandom random,
        out string failure)
    {
        failure = string.Empty;
        foreach (MapRect room in candidate.Rooms)
        {
            foreach (RandomMapPoint point in EnumerateRoomRing(room))
            {
                candidate.Walls[point.Y, point.X] = true;
            }
        }

        foreach (IReadOnlyList<RandomMapPoint> openings in candidate.RoomDoors.Values)
        {
            foreach (RandomMapPoint opening in openings)
            {
                candidate.Walls[opening.Y, opening.X] = false;
            }
        }

        foreach (RandomMapPoint marker in markers)
        {
            if (candidate.Rooms.Any(room => IsOnRoomRing(room, marker)))
            {
                failure = "noRoomDoors";
                return false;
            }
        }

        HashSet<RandomMapPoint> protectedRoomRings = CollectRoomRingWalls(candidate);
        if (!ConnectEmptyComponents(candidate.Walls, protectedRoomRings))
        {
            failure = "noRoomDoors";
            return false;
        }

        foreach (MapRect room in candidate.Rooms)
        {
            List<RandomMapPoint> openings = EnumerateRoomRing(room)
                .Where(
                    point => !candidate.Walls[point.Y, point.X]
                        && IsValidRoomDoor(candidate.Walls, room, point))
                .ToList();
            if (openings.Count == 1)
            {
                candidate.RoomDoors[room] = openings;
                continue;
            }

            if (openings.Count != 2
                || !AreOppositeRoomOpenings(room, openings[0], openings[1]))
            {
                RandomMapPoint primary = candidate.RoomDoors[room][0];
                RandomMapPoint opposite = GetOppositeRoomDoor(room, primary);

                foreach (RandomMapPoint point in EnumerateRoomRing(room))
                {
                    candidate.Walls[point.Y, point.X] = true;
                }

                candidate.Walls[primary.Y, primary.X] = false;
                if (!IsValidRoomDoor(candidate.Walls, room, opposite))
                {
                    candidate.RoomDoors[room] = [primary];
                    continue;
                }

                candidate.Walls[opposite.Y, opposite.X] = false;
                List<RandomMapPoint> promotedDoors = [primary, opposite];
                candidate.RoomDoors[room] = promotedDoors;
                AddRoomPassThroughToMainPath(candidate, room, promotedDoors);
                ConnectPromotedRoomDoor(
                    candidate,
                    room,
                    opposite,
                    random);
                continue;
            }

            candidate.RoomDoors[room] = openings;
            AddRoomPassThroughToMainPath(candidate, room, openings);
        }

        if (!FillDetachedEmptyArtifacts(candidate, markers))
        {
            failure = "noRoomDoors";
            return false;
        }

        return true;
    }

    private static bool FillDetachedEmptyArtifacts(
        GenerationCandidate candidate,
        HashSet<RandomMapPoint> markers)
    {
        int[,] components = LabelEmptyComponents(
            candidate.Walls,
            out int componentCount);
        if (componentCount <= 1)
        {
            return componentCount == 1;
        }

        RandomMapPoint? anchor = markers.Count > 0 ? markers.First() : null;
        if (anchor is null)
        {
            anchor = candidate.MainPath
                .Where(point => !candidate.Walls[point.Y, point.X])
                .Select(point => (RandomMapPoint?)point)
                .FirstOrDefault();
        }

        anchor ??= FindFirstPlayableEmpty(candidate.Walls);
        if (anchor is null)
        {
            return false;
        }

        int retainedComponent = components[anchor.Value.Y, anchor.Value.X];
        if (retainedComponent < 0
            || markers.Any(marker => components[marker.Y, marker.X] != retainedComponent)
            || candidate.Rooms.Any(
                room => components[room.Center.Y, room.Center.X] != retainedComponent))
        {
            return false;
        }

        int height = candidate.Walls.GetLength(0);
        int width = candidate.Walls.GetLength(1);
        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (!candidate.Walls[y, x]
                    && components[y, x] != retainedComponent)
                {
                    candidate.Walls[y, x] = true;
                }
            }
        }

        return HasSingleEmptyComponent(candidate.Walls);
    }

    private static RandomMapPoint GetOppositeRoomDoor(
        MapRect room,
        RandomMapPoint door)
    {
        RoomSide side = GetRoomSide(room, door);
        return side switch
        {
            RoomSide.Top => new RandomMapPoint(door.X, room.Bottom),
            RoomSide.Bottom => new RandomMapPoint(door.X, room.Y - 1),
            RoomSide.Left => new RandomMapPoint(room.Right, door.Y),
            _ => new RandomMapPoint(room.X - 1, door.Y),
        };
    }

    private static void ConnectPromotedRoomDoor(
        GenerationCandidate candidate,
        MapRect room,
        RandomMapPoint door,
        StableRandom random)
    {
        HashSet<RandomMapPoint> avoidedCells = [];
        foreach (MapRect otherRoom in candidate.Rooms)
        {
            if (otherRoom == room)
            {
                continue;
            }

            avoidedCells.UnionWith(EnumerateRoomAreaAndRing(otherRoom));
        }

        RandomMapPoint outside = GetOutsideRoomPoint(room, door);
        List<RandomMapPoint> mainPath = candidate.MainPath.ToList();
        RandomMapPoint target = FindNearest(outside, mainPath);
        List<RandomMapPoint> connection = FindOrthogonalPath(
            candidate.Walls,
            outside,
            target,
            random,
            false,
            avoidedCells);
        OpenPath(candidate.Walls, connection);
        candidate.MainPath.UnionWith(connection);
    }

    private static void AddRoomPassThroughToMainPath(
        GenerationCandidate candidate,
        MapRect room,
        IReadOnlyList<RandomMapPoint> openings)
    {
        RandomMapPoint first = openings[0];
        RandomMapPoint second = openings[1];

        if (first.X == second.X)
        {
            for (int y = room.Y; y < room.Bottom; y++)
            {
                candidate.MainPath.Add(new RandomMapPoint(first.X, y));
            }
        }
        else
        {
            for (int x = room.X; x < room.Right; x++)
            {
                candidate.MainPath.Add(new RandomMapPoint(x, first.Y));
            }
        }

        candidate.MainPath.Add(first);
        candidate.MainPath.Add(second);
    }

    private static bool RepairEmptyWindows(
        bool[,] walls,
        HashSet<RandomMapPoint> protectedEmpty,
        int windowSize,
        StableRandom random)
    {
        int maximumRepairs = GetPlayableArea(walls) * 2;

        for (int repair = 0; repair < maximumRepairs; repair++)
        {
            if (!TryFindEmptyWindow(walls, windowSize, out MapRect window))
            {
                return true;
            }

            List<RandomMapPoint> candidates = [];
            for (int y = window.Y; y < window.Bottom; y++)
            {
                for (int x = window.X; x < window.Right; x++)
                {
                    RandomMapPoint point = new RandomMapPoint(x, y);
                    if (IsInterior(
                            x,
                            y,
                            walls.GetLength(1),
                            walls.GetLength(0))
                        && !protectedEmpty.Contains(point)
                        && !WouldCreateWallBlock(walls, x, y))
                    {
                        candidates.Add(point);
                    }
                }
            }

            random.Shuffle(candidates);
            candidates.Sort(
                (left, right) => CountOpenNeighbors(walls, right.X, right.Y)
                    .CompareTo(CountOpenNeighbors(walls, left.X, left.Y)));
            bool repaired = false;

            foreach (RandomMapPoint candidate in candidates)
            {
                walls[candidate.Y, candidate.X] = true;
                if (HasSingleEmptyComponent(walls)
                    && !HasWallBlock(walls))
                {
                    repaired = true;
                    break;
                }

                walls[candidate.Y, candidate.X] = false;
            }

            if (!repaired)
            {
                return false;
            }
        }

        return false;
    }

    private static bool ReduceWalls(
        bool[,] walls,
        int targetWalls,
        int windowSize,
        HashSet<RandomMapPoint> protectedWalls,
        StableRandom random)
    {
        int wallCount = CountWalls(walls);
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        while (wallCount > targetWalls)
        {
            List<RandomMapPoint> candidates = [];

            for (int y = 1; y < height - 1; y++)
            {
                for (int x = 1; x < width - 1; x++)
                {
                    if (walls[y, x]
                        && !protectedWalls.Contains(new RandomMapPoint(x, y))
                        && CountOpenNeighbors(walls, x, y) > 0)
                    {
                        candidates.Add(new RandomMapPoint(x, y));
                    }
                }
            }

            if (candidates.Count == 0)
            {
                return false;
            }

            random.Shuffle(candidates);
            candidates.Sort(
                (left, right) => CountWallNeighbors(walls, right.X, right.Y)
                    .CompareTo(CountWallNeighbors(walls, left.X, left.Y)));
            int opened = 0;

            foreach (RandomMapPoint candidate in candidates)
            {
                if (wallCount <= targetWalls)
                {
                    break;
                }

                if (!walls[candidate.Y, candidate.X]
                    || CountOpenNeighbors(walls, candidate.X, candidate.Y) == 0)
                {
                    continue;
                }

                walls[candidate.Y, candidate.X] = false;
                if (!WouldCreateEmptyWindow(walls, candidate.X, candidate.Y, windowSize))
                {
                    wallCount--;
                    opened++;
                    continue;
                }

                walls[candidate.Y, candidate.X] = true;
            }

            if (opened == 0)
            {
                return false;
            }
        }

        return true;
    }
}
