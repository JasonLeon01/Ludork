using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public enum RandomMapMode
{
    Lattice,
    Rooms,
}

public readonly record struct RandomMapPoint(int X, int Y);

public sealed record RandomMapGenerationResult(
    bool Success,
    string Error,
    IReadOnlyList<IReadOnlyList<int?>> Tiles,
    int Seed);

public static class RandomMapGenerator
{
    private const int CandidateCount = 32;

    private static readonly RandomMapPoint[] Directions =
    [
        new RandomMapPoint(1, 0),
        new RandomMapPoint(0, 1),
        new RandomMapPoint(-1, 0),
        new RandomMapPoint(0, -1),
    ];

    public static RandomMapGenerationResult Generate(
        int width,
        int height,
        int wallTile,
        IEnumerable<RandomMapPoint> markers,
        RandomMapMode mode,
        int seed,
        CancellationToken cancellationToken = default)
    {
        return Generate(
            width,
            height,
            wallTile,
            markers,
            mode,
            50,
            seed,
            cancellationToken);
    }

    public static RandomMapGenerationResult Generate(
        int width,
        int height,
        int wallTile,
        IEnumerable<RandomMapPoint> markers,
        RandomMapMode mode,
        int densityPercent,
        int seed,
        CancellationToken cancellationToken = default)
    {
        string? validationError = ValidateInput(
            width,
            height,
            markers,
            mode,
            densityPercent,
            out HashSet<RandomMapPoint> markerSet);
        if (validationError is not null)
        {
            return Failure(validationError, seed);
        }

        HashSet<RandomMapPoint> workingMarkers = markerSet
            .Select(marker => new RandomMapPoint(marker.X + 1, marker.Y + 1))
            .ToHashSet();
        int workingWidth = width + 2;
        int workingHeight = height + 2;
        int targetWalls = (int)((long)width * height * densityPercent / 100);
        bool[,]? bestWalls = null;
        int bestWallCount = -1;
        double bestScore = double.NegativeInfinity;
        Dictionary<string, int> failures = [];

        int candidateCount = GetCandidateCount(width, height);
        for (int candidateIndex = 0; candidateIndex < candidateCount; candidateIndex++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            StableRandom random = new StableRandom(DeriveSeed(seed, candidateIndex));
            GenerationCandidate candidate = mode == RandomMapMode.Lattice
                ? GenerateLattice(workingWidth, workingHeight, random)
                : GenerateRooms(workingWidth, workingHeight, workingMarkers, random);

            if (!RepairAndValidate(
                candidate,
                workingMarkers,
                mode,
                targetWalls,
                random,
                out string failure))
            {
                failures[failure] = failures.GetValueOrDefault(failure) + 1;
                continue;
            }

            cancellationToken.ThrowIfCancellationRequested();
            int wallCount = CountWalls(candidate.Walls);
            double score = Score(candidate, mode, targetWalls);
            int densityTolerance = Math.Max(1, GetPlayableArea(candidate.Walls) / 100);
            if (bestWalls is null
                || wallCount > bestWallCount + densityTolerance
                || (wallCount + densityTolerance >= bestWallCount
                    && score > bestScore))
            {
                bestWalls = candidate.Walls;
                bestWallCount = wallCount;
                bestScore = score;
            }
        }

        cancellationToken.ThrowIfCancellationRequested();

        if (bestWalls is null)
        {
            return Failure(failures.OrderByDescending(pair => pair.Value).First().Key, seed);
        }

        return new RandomMapGenerationResult(
            true,
            string.Empty,
            CreateTiles(bestWalls, wallTile),
            seed);
    }

    private static int GetCandidateCount(int width, int height)
    {
        int area = width * height;
        if (area >= 1024)
        {
            return 8;
        }

        if (area >= 400)
        {
            return 16;
        }

        return CandidateCount;
    }

    private static string? ValidateInput(
        int width,
        int height,
        IEnumerable<RandomMapPoint> markers,
        RandomMapMode mode,
        int densityPercent,
        out HashSet<RandomMapPoint> markerSet)
    {
        markerSet = [];

        if (width <= 0 || height <= 0)
        {
            return "invalidDimensions";
        }

        if (width > int.MaxValue - 2
            || height > int.MaxValue - 2
            || (long)width * height > int.MaxValue
            || (long)(width + 2) * (height + 2) > int.MaxValue)
        {
            return "invalidDimensions";
        }

        if (markers is null)
        {
            return "invalidMarkers";
        }

        if (mode != RandomMapMode.Lattice && mode != RandomMapMode.Rooms)
        {
            return "invalidMode";
        }

        if (densityPercent < 35 || densityPercent > 75)
        {
            return "invalidDensity";
        }

        if (mode == RandomMapMode.Lattice && (width < 7 || height < 7))
        {
            return "latticeTooSmall";
        }

        if (mode == RandomMapMode.Rooms
            && (Math.Min(width, height) < 7 || (long)width * height < 81))
        {
            return "roomsTooSmall";
        }

        foreach (RandomMapPoint marker in markers)
        {
            if (marker.X < 0 || marker.Y < 0 || marker.X >= width || marker.Y >= height)
            {
                return "markerOutOfBounds";
            }

            markerSet.Add(marker);
        }

        return null;
    }

    private static GenerationCandidate GenerateLattice(int width, int height, StableRandom random)
    {
        bool[,] walls = CreateFilledWalls(width, height);
        List<RandomMapPoint> nodes = [];
        int latticeStartX = random.Next(2) + 1;
        int latticeStartY = random.Next(2) + 1;

        for (int y = latticeStartY; y < height - 1; y += 2)
        {
            for (int x = latticeStartX; x < width - 1; x += 2)
            {
                nodes.Add(new RandomMapPoint(x, y));
            }
        }

        RandomMapPoint root = nodes[random.Next(nodes.Count)];
        HashSet<RandomMapPoint> visited = [root];
        Dictionary<RandomMapPoint, int> arrivalDirections = [];
        Dictionary<RandomMapPoint, int> degrees = [];
        List<LatticeEdge> frontier = [];
        walls[root.Y, root.X] = false;
        AddLatticeFrontier(root, width, height, visited, frontier);

        while (frontier.Count > 0)
        {
            int totalWeight = 0;
            int[] weights = new int[frontier.Count];

            for (int index = 0; index < frontier.Count; index++)
            {
                LatticeEdge edge = frontier[index];
                int directionWeight = arrivalDirections.TryGetValue(edge.From, out int arrival)
                    && arrival == edge.Direction
                    ? 2
                    : 7;
                int branchWeight = degrees.GetValueOrDefault(edge.From) <= 1 ? 5 : 2;
                int weight = directionWeight + branchWeight + random.Next(4);
                weights[index] = weight;
                totalWeight += weight;
            }

            int selection = random.Next(totalWeight);
            int selectedIndex = 0;

            for (; selectedIndex < weights.Length - 1; selectedIndex++)
            {
                if (selection < weights[selectedIndex])
                {
                    break;
                }

                selection -= weights[selectedIndex];
            }

            LatticeEdge selected = frontier[selectedIndex];
            frontier.RemoveAt(selectedIndex);
            if (visited.Contains(selected.To))
            {
                continue;
            }

            RandomMapPoint direction = Directions[selected.Direction];
            walls[selected.From.Y + direction.Y, selected.From.X + direction.X] = false;
            walls[selected.To.Y, selected.To.X] = false;
            visited.Add(selected.To);
            arrivalDirections[selected.To] = selected.Direction;
            degrees[selected.From] = degrees.GetValueOrDefault(selected.From) + 1;
            degrees[selected.To] = degrees.GetValueOrDefault(selected.To) + 1;
            AddLatticeFrontier(selected.To, width, height, visited, frontier);
        }

        List<LatticeEdge> loopEdges = [];
        foreach (RandomMapPoint node in nodes)
        {
            for (int directionIndex = 0; directionIndex < 2; directionIndex++)
            {
                RandomMapPoint direction = Directions[directionIndex];
                RandomMapPoint other = new RandomMapPoint(
                    node.X + direction.X * 2,
                    node.Y + direction.Y * 2);

                if (!IsInterior(other.X, other.Y, width, height))
                {
                    continue;
                }

                int connectorX = node.X + direction.X;
                int connectorY = node.Y + direction.Y;
                if (walls[connectorY, connectorX])
                {
                    loopEdges.Add(new LatticeEdge(node, other, directionIndex));
                }
            }
        }

        random.Shuffle(loopEdges);
        int loopPercent = random.NextInclusive(24, 40);
        int loopCount = (loopEdges.Count * loopPercent + 99) / 100;

        for (int index = 0; index < loopCount && index < loopEdges.Count; index++)
        {
            LatticeEdge edge = loopEdges[index];
            RandomMapPoint direction = Directions[edge.Direction];
            walls[edge.From.Y + direction.Y, edge.From.X + direction.X] = false;
        }

        int roomCount = Math.Max(1, nodes.Count / 14);
        random.Shuffle(nodes);

        for (int index = 0; index < roomCount && index < nodes.Count; index++)
        {
            RandomMapPoint center = nodes[index];
            int roomWidth = random.Next(4) == 0 ? 3 : 2;
            int roomHeight = random.Next(4) == 0 ? 3 : 2;
            int startX = Math.Clamp(center.X - random.Next(roomWidth), 1, width - roomWidth - 1);
            int startY = Math.Clamp(center.Y - random.Next(roomHeight), 1, height - roomHeight - 1);

            for (int y = startY; y < startY + roomHeight; y++)
            {
                for (int x = startX; x < startX + roomWidth; x++)
                {
                    walls[y, x] = false;
                }
            }
        }

        SealVirtualBoundary(walls);
        return new GenerationCandidate(
            walls,
            nodes.Count,
            [],
            [],
            new Dictionary<MapRect, IReadOnlyList<RandomMapPoint>>(),
            []);
    }

    private static void AddLatticeFrontier(
        RandomMapPoint node,
        int width,
        int height,
        HashSet<RandomMapPoint> visited,
        List<LatticeEdge> frontier)
    {
        for (int directionIndex = 0; directionIndex < Directions.Length; directionIndex++)
        {
            RandomMapPoint direction = Directions[directionIndex];
            RandomMapPoint other = new RandomMapPoint(
                node.X + direction.X * 2,
                node.Y + direction.Y * 2);

            if (IsInterior(other.X, other.Y, width, height) && !visited.Contains(other))
            {
                frontier.Add(new LatticeEdge(node, other, directionIndex));
            }
        }
    }

    private static GenerationCandidate GenerateRooms(
        int width,
        int height,
        HashSet<RandomMapPoint> markers,
        StableRandom random)
    {
        bool[,] walls = CreateFilledWalls(width, height);
        List<RandomMapPoint> route = BuildMarkerRoute(width, height, markers, random);
        List<RandomMapPoint> mainPath = [];

        for (int index = 0; index < route.Count - 1; index++)
        {
            List<RandomMapPoint> segment = FindOrthogonalPath(
                walls,
                route[index],
                route[index + 1],
                random,
                true,
                null);
            OpenPath(walls, segment);

            if (index == 0)
            {
                mainPath.AddRange(segment);
            }
            else
            {
                mainPath.AddRange(segment.Skip(1));
            }
        }

        if (mainPath.Count == 0)
        {
            mainPath.Add(route[0]);
            walls[route[0].Y, route[0].X] = false;
        }

        EnsureMainPathRun(
            walls,
            mainPath,
            Math.Max(4, Math.Max(width - 2, height - 2) / 4),
            [],
            random);

        HashSet<RandomMapPoint> mainPathExclusion = mainPath.ToHashSet();
        if (GetPlayableArea(walls) > 100)
        {
            foreach (RandomMapPoint point in mainPath.ToList())
            {
                foreach (RandomMapPoint direction in Directions)
                {
                    mainPathExclusion.Add(
                        new RandomMapPoint(
                            point.X + direction.X,
                            point.Y + direction.Y));
                }
            }
        }

        List<MapRect> leaves = [];
        SplitRegion(new MapRect(1, 1, width - 2, height - 2), random, leaves);
        random.Shuffle(leaves);
        int playableArea = GetPlayableArea(walls);
        int maximumRoomCount = Math.Clamp((playableArea + 47) / 48, 2, 28);
        if (leaves.Count > maximumRoomCount)
        {
            leaves = leaves.Take(maximumRoomCount).ToList();
        }

        List<MapRect> rooms = [];
        HashSet<RandomMapPoint> occupiedRoomExclusion = [];
        List<MapRect> globalFallbackRoomOptions = [];

        foreach (MapRect leaf in leaves)
        {
            int horizontalInsets = (leaf.X > 1 ? 1 : 0)
                + (leaf.Right < width - 1 ? 1 : 0);
            int verticalInsets = (leaf.Y > 1 ? 1 : 0)
                + (leaf.Bottom < height - 1 ? 1 : 0);
            int maximumWidth = Math.Min(5, leaf.Width - horizontalInsets);
            int maximumHeight = Math.Min(5, leaf.Height - verticalInsets);
            List<MapRect> roomOptions = [];
            List<MapRect> fallbackRoomOptions = [];

            for (int roomHeight = 3; roomHeight <= maximumHeight; roomHeight++)
            {
                for (int roomWidth = 3; roomWidth <= maximumWidth; roomWidth++)
                {
                    if (roomWidth * roomHeight > 20)
                    {
                        continue;
                    }

                    int minimumRoomY = leaf.Y == 1 ? leaf.Y : leaf.Y + 1;
                    int maximumRoomY = leaf.Bottom == height - 1
                        ? leaf.Bottom - roomHeight
                        : leaf.Bottom - roomHeight - 1;
                    int minimumRoomX = leaf.X == 1 ? leaf.X : leaf.X + 1;
                    int maximumRoomX = leaf.Right == width - 1
                        ? leaf.Right - roomWidth
                        : leaf.Right - roomWidth - 1;

                    for (int roomY = minimumRoomY;
                        roomY <= maximumRoomY;
                        roomY++)
                    {
                        for (int roomX = minimumRoomX;
                            roomX <= maximumRoomX;
                            roomX++)
                        {
                            MapRect option = new MapRect(
                                roomX,
                                roomY,
                                roomWidth,
                                roomHeight);
                            if (!HasRoomRingOnActualEdge(option, width, height)
                                && !markers.Any(marker => IsOnRoomRing(option, marker))
                                && (playableArea == 81
                                    || !markers.Any(option.Contains))
                                && !EnumerateRoomRing(option).Any(occupiedRoomExclusion.Contains))
                            {
                                fallbackRoomOptions.Add(option);
                                if (!EnumerateRoomAreaAndRing(option)
                                    .Any(mainPathExclusion.Contains))
                                {
                                    roomOptions.Add(option);
                                }
                            }
                        }
                    }
                }
            }

            if (roomOptions.Count == 0)
            {
                globalFallbackRoomOptions.AddRange(fallbackRoomOptions);
                roomOptions = fallbackRoomOptions;
            }

            if (roomOptions.Count == 0)
            {
                continue;
            }

            List<MapRect> markerFreeOptions = roomOptions
                .Where(option => !markers.Any(option.Contains))
                .ToList();
            if (markerFreeOptions.Count > 0)
            {
                roomOptions = markerFreeOptions;
            }

            int preferredArea = random.Next(5) == 0 ? 20 : 12;
            List<MapRect> compactOptions = roomOptions
                .Where(option => option.Width * option.Height <= preferredArea)
                .ToList();
            if (compactOptions.Count > 0)
            {
                roomOptions = compactOptions;
            }

            MapRect room = SelectWeightedRoomOption(
                roomOptions,
                width,
                height,
                random);
            rooms.Add(room);
            foreach (RandomMapPoint ringPoint in EnumerateRoomRing(room))
            {
                occupiedRoomExclusion.Add(ringPoint);
                foreach (RandomMapPoint direction in Directions)
                {
                    occupiedRoomExclusion.Add(
                        new RandomMapPoint(
                            ringPoint.X + direction.X,
                            ringPoint.Y + direction.Y));
                }
            }

            OpenRectangle(walls, room);
        }

        if (rooms.Count == 0 && globalFallbackRoomOptions.Count > 0)
        {
            MapRect room = SelectWeightedRoomOption(
                globalFallbackRoomOptions,
                width,
                height,
                random);
            rooms.Add(room);
            OpenRectangle(walls, room);
        }

        HashSet<RandomMapPoint> avoidedMainPathCells = [];
        foreach (MapRect room in rooms)
        {
            for (int y = room.Y - 1; y <= room.Bottom; y++)
            {
                for (int x = room.X - 1; x <= room.Right; x++)
                {
                    avoidedMainPathCells.Add(new RandomMapPoint(x, y));
                }
            }
        }

        if (rooms.Any(
            room => EnumerateRoomAreaAndRing(room).Any(mainPath.Contains)))
        {
            foreach (RandomMapPoint point in mainPath)
            {
                walls[point.Y, point.X] = true;
            }

            foreach (MapRect room in rooms)
            {
                OpenRectangle(walls, room);
            }

            mainPath.Clear();
            for (int index = 0; index < route.Count - 1; index++)
            {
                HashSet<RandomMapPoint> segmentAvoidedCells =
                    CreateSegmentAvoidedCells(
                        avoidedMainPathCells,
                        rooms,
                        route[index],
                        route[index + 1]);
                List<RandomMapPoint> segment = FindOrthogonalPath(
                    walls,
                    route[index],
                    route[index + 1],
                    random,
                    true,
                    segmentAvoidedCells);
                OpenPath(walls, segment);

                if (index == 0)
                {
                    mainPath.AddRange(segment);
                }
                else
                {
                    mainPath.AddRange(segment.Skip(1));
                }
            }

            EnsureMainPathRun(
                walls,
                mainPath,
                Math.Max(4, Math.Max(width - 2, height - 2) / 4),
                avoidedMainPathCells,
                random);
        }

        Dictionary<MapRect, List<RandomMapPoint>> roomCorridors = [];
        foreach (MapRect room in rooms)
        {
            RandomMapPoint center = room.Center;
            RandomMapPoint target = FindNearest(center, mainPath);
            HashSet<RandomMapPoint> avoidedCorridorCells =
                new HashSet<RandomMapPoint>(avoidedMainPathCells);
            for (int y = room.Y - 1; y <= room.Bottom; y++)
            {
                for (int x = room.X - 1; x <= room.Right; x++)
                {
                    avoidedCorridorCells.Remove(new RandomMapPoint(x, y));
                }
            }

            List<RandomMapPoint> corridor = FindOrthogonalPath(
                walls,
                center,
                target,
                random,
                false,
                avoidedCorridorCells);
            OpenPath(walls, corridor);
            roomCorridors[room] = corridor;
        }

        Dictionary<MapRect, IReadOnlyList<RandomMapPoint>> roomDoors =
            NormalizeRoomRings(walls, rooms, mainPath, roomCorridors);
        EnsureRoomDoorConnections(
            walls,
            roomDoors,
            mainPath,
            avoidedMainPathCells,
            random);
        HashSet<RandomMapPoint> roomNotches = CreateRoomNotches(
            walls,
            rooms,
            roomDoors,
            roomCorridors,
            mainPath,
            markers,
            random);
        AddRoomNetworkLoops(walls, rooms, mainPath, random);
        SealVirtualBoundary(walls);
        return new GenerationCandidate(
            walls,
            0,
            rooms,
            mainPath.ToHashSet(),
            roomDoors,
            roomNotches);
    }

    private static HashSet<RandomMapPoint> CreateRoomNotches(
        bool[,] walls,
        IReadOnlyList<MapRect> rooms,
        IReadOnlyDictionary<MapRect, IReadOnlyList<RandomMapPoint>> roomDoors,
        IReadOnlyDictionary<MapRect, List<RandomMapPoint>> roomCorridors,
        IReadOnlyList<RandomMapPoint> mainPath,
        HashSet<RandomMapPoint> markers,
        StableRandom random)
    {
        HashSet<RandomMapPoint> excluded =
        [
            .. mainPath,
            .. markers,
        ];
        foreach (IReadOnlyList<RandomMapPoint> doors in roomDoors.Values)
        {
            excluded.UnionWith(doors);
        }

        foreach (IReadOnlyList<RandomMapPoint> corridor in roomCorridors.Values)
        {
            excluded.UnionWith(corridor);
        }

        List<MapRect> shuffledRooms = rooms.ToList();
        random.Shuffle(shuffledRooms);
        int targetCount = rooms.Count == 0
            ? 0
            : Math.Max(1, (rooms.Count + 1) / 2);
        HashSet<RandomMapPoint> notches = [];

        foreach (MapRect room in shuffledRooms)
        {
            if (notches.Count >= targetCount)
            {
                break;
            }

            List<RandomMapPoint> candidates = [];
            for (int x = room.X + 1; x < room.Right - 1; x++)
            {
                candidates.Add(new RandomMapPoint(x, room.Y));
                candidates.Add(new RandomMapPoint(x, room.Bottom - 1));
            }

            for (int y = room.Y + 1; y < room.Bottom - 1; y++)
            {
                candidates.Add(new RandomMapPoint(room.X, y));
                candidates.Add(new RandomMapPoint(room.Right - 1, y));
            }

            random.Shuffle(candidates);
            foreach (RandomMapPoint candidate in candidates)
            {
                if (excluded.Contains(candidate))
                {
                    continue;
                }

                walls[candidate.Y, candidate.X] = true;
                if (!WouldCreateWallBlock(walls, candidate.X, candidate.Y)
                    && IsRoomInteriorConnected(walls, room))
                {
                    notches.Add(candidate);
                    break;
                }

                walls[candidate.Y, candidate.X] = false;
            }
        }

        return notches;
    }

    private static bool IsRoomInteriorConnected(bool[,] walls, MapRect room)
    {
        RandomMapPoint? start = null;
        int emptyCount = 0;

        for (int y = room.Y; y < room.Bottom; y++)
        {
            for (int x = room.X; x < room.Right; x++)
            {
                if (walls[y, x])
                {
                    continue;
                }

                start ??= new RandomMapPoint(x, y);
                emptyCount++;
            }
        }

        if (start is null)
        {
            return false;
        }

        HashSet<RandomMapPoint> visited = [start.Value];
        Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();
        queue.Enqueue(start.Value);

        while (queue.Count > 0)
        {
            RandomMapPoint current = queue.Dequeue();
            foreach (RandomMapPoint direction in Directions)
            {
                RandomMapPoint next = new RandomMapPoint(
                    current.X + direction.X,
                    current.Y + direction.Y);
                if (room.Contains(next)
                    && !walls[next.Y, next.X]
                    && visited.Add(next))
                {
                    queue.Enqueue(next);
                }
            }
        }

        return visited.Count == emptyCount;
    }

    private static void AddRoomNetworkLoops(
        bool[,] walls,
        IReadOnlyList<MapRect> rooms,
        IReadOnlyList<RandomMapPoint> mainPath,
        StableRandom random)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        HashSet<RandomMapPoint> avoidedCells = [];
        HashSet<RandomMapPoint> roomInteriors = [];
        foreach (MapRect room in rooms)
        {
            avoidedCells.UnionWith(EnumerateRoomAreaAndRing(room));
            for (int y = room.Y; y < room.Bottom; y++)
            {
                for (int x = room.X; x < room.Right; x++)
                {
                    roomInteriors.Add(new RandomMapPoint(x, y));
                }
            }
        }

        for (int x = 0; x < width; x++)
        {
            avoidedCells.Add(new RandomMapPoint(x, 0));
            avoidedCells.Add(new RandomMapPoint(x, height - 1));
        }

        for (int y = 1; y < height - 1; y++)
        {
            avoidedCells.Add(new RandomMapPoint(0, y));
            avoidedCells.Add(new RandomMapPoint(width - 1, y));
        }

        int targetCycles = Math.Clamp((rooms.Count + 3) / 4, 1, 6);
        int attempts = Math.Max(20, targetCycles * 12);
        int initialCycles =
            CalculateInteriorGraphMetrics(walls, roomInteriors).CycleRank;

        for (int attempt = 0; attempt < attempts; attempt++)
        {
            InteriorGraphMetrics currentMetrics =
                CalculateInteriorGraphMetrics(walls, roomInteriors);
            if (currentMetrics.CycleRank >= initialCycles + targetCycles)
            {
                return;
            }

            if (TryOpenParallelMainPathLoop(
                walls,
                mainPath,
                avoidedCells,
                roomInteriors,
                currentMetrics.CycleRank,
                random))
            {
                continue;
            }

            List<RandomMapPoint> network = [];
            for (int y = 1; y < height - 1; y++)
            {
                for (int x = 1; x < width - 1; x++)
                {
                    RandomMapPoint point = new RandomMapPoint(x, y);
                    if (!walls[y, x]
                        && !avoidedCells.Contains(point))
                    {
                        network.Add(point);
                    }
                }
            }

            if (network.Count < 2)
            {
                return;
            }

            Dictionary<RandomMapPoint, int> components =
                LabelInteriorGraphComponents(walls, roomInteriors);
            List<RandomMapPoint> shortcutWalls = [];
            for (int y = 1; y < height - 1; y++)
            {
                for (int x = 1; x < width - 1; x++)
                {
                    RandomMapPoint point = new RandomMapPoint(x, y);
                    if (!walls[y, x] || avoidedCells.Contains(point))
                    {
                        continue;
                    }

                    List<int> neighborComponents = [];
                    foreach (RandomMapPoint direction in Directions)
                    {
                        RandomMapPoint neighbor = new RandomMapPoint(
                            x + direction.X,
                            y + direction.Y);
                        if (components.TryGetValue(neighbor, out int component))
                        {
                            neighborComponents.Add(component);
                        }
                    }

                    if (neighborComponents
                        .GroupBy(component => component)
                        .Any(group => group.Count() >= 2))
                    {
                        shortcutWalls.Add(point);
                    }
                }
            }

            if (shortcutWalls.Count > 0)
            {
                random.Shuffle(shortcutWalls);
                shortcutWalls.Sort(
                    (left, right) => CountOpenNeighbors(walls, left.X, left.Y)
                        .CompareTo(CountOpenNeighbors(walls, right.X, right.Y)));
                RandomMapPoint shortcut = shortcutWalls[0];
                walls[shortcut.Y, shortcut.X] = false;
                InteriorGraphMetrics shortcutMetrics =
                    CalculateInteriorGraphMetrics(walls, roomInteriors);
                if (shortcutMetrics.CycleRank > currentMetrics.CycleRank)
                {
                    continue;
                }

                walls[shortcut.Y, shortcut.X] = true;
            }

            List<List<RandomMapPoint>> groups = network
                .Where(components.ContainsKey)
                .GroupBy(point => components[point])
                .Select(group => group.ToList())
                .Where(group => group.Count >= 2)
                .ToList();
            if (groups.Count == 0)
            {
                return;
            }

            random.Shuffle(groups);
            List<RandomMapPoint> group = groups
                .OrderByDescending(points => points.Count)
                .First();
            RandomMapPoint first = group[random.Next(group.Count)];
            List<RandomMapPoint> distant = group
                .Where(
                    point => Manhattan(first, point)
                        >= Math.Max(3, Math.Min(width - 2, height - 2) / 4))
                .ToList();
            if (distant.Count == 0)
            {
                continue;
            }

            RandomMapPoint second = distant[random.Next(distant.Count)];
            List<RandomMapPoint> path = FindOrthogonalPath(
                walls,
                first,
                second,
                random,
                true,
                avoidedCells);
            int openedWalls = path.Count(point => walls[point.Y, point.X]);
            if (openedWalls < 1)
            {
                continue;
            }

            List<RandomMapPoint> changed = path
                .Where(point => walls[point.Y, point.X])
                .ToList();
            OpenPath(walls, changed);
            InteriorGraphMetrics nextMetrics =
                CalculateInteriorGraphMetrics(walls, roomInteriors);
            if (nextMetrics.CycleRank <= currentMetrics.CycleRank)
            {
                foreach (RandomMapPoint point in changed)
                {
                    walls[point.Y, point.X] = true;
                }
            }
        }
    }

    private static bool TryOpenParallelMainPathLoop(
        bool[,] walls,
        IReadOnlyList<RandomMapPoint> mainPath,
        HashSet<RandomMapPoint> avoidedCells,
        HashSet<RandomMapPoint> roomInteriors,
        int currentCycles,
        StableRandom random)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        HashSet<RandomMapPoint> pathSet = mainPath.ToHashSet();
        List<List<RandomMapPoint>> candidates = [];

        for (int y = 1; y < height - 1; y++)
        {
            int start = 1;
            while (start < width - 1)
            {
                while (start < width - 1
                    && !pathSet.Contains(new RandomMapPoint(start, y)))
                {
                    start++;
                }

                int end = start;
                while (end + 1 < width - 1
                    && pathSet.Contains(new RandomMapPoint(end + 1, y)))
                {
                    end++;
                }

                for (int segmentStart = start;
                    segmentStart + 3 <= end;
                    segmentStart += 2)
                {
                    int segmentEnd = Math.Min(end, segmentStart + 7);
                    for (int offset = -2; offset <= 2; offset++)
                    {
                        if (offset == 0)
                        {
                            continue;
                        }

                        int parallelY = y + offset;
                        if (parallelY <= 0 || parallelY >= height - 1)
                        {
                            continue;
                        }

                        List<RandomMapPoint> loop = [];
                        int step = Math.Sign(offset);
                        for (int loopY = y; loopY != parallelY + step; loopY += step)
                        {
                            loop.Add(new RandomMapPoint(segmentStart, loopY));
                        }

                        for (int loopX = segmentStart + 1;
                            loopX <= segmentEnd;
                            loopX++)
                        {
                            loop.Add(new RandomMapPoint(loopX, parallelY));
                        }

                        for (int loopY = parallelY - step;
                            loopY != y - step;
                            loopY -= step)
                        {
                            loop.Add(new RandomMapPoint(segmentEnd, loopY));
                        }

                        if (!loop.Any(avoidedCells.Contains))
                        {
                            candidates.Add(loop);
                        }
                    }
                }

                start = Math.Max(start + 1, end + 1);
            }
        }

        for (int x = 1; x < width - 1; x++)
        {
            int start = 1;
            while (start < height - 1)
            {
                while (start < height - 1
                    && !pathSet.Contains(new RandomMapPoint(x, start)))
                {
                    start++;
                }

                int end = start;
                while (end + 1 < height - 1
                    && pathSet.Contains(new RandomMapPoint(x, end + 1)))
                {
                    end++;
                }

                for (int segmentStart = start;
                    segmentStart + 3 <= end;
                    segmentStart += 2)
                {
                    int segmentEnd = Math.Min(end, segmentStart + 7);
                    for (int offset = -2; offset <= 2; offset++)
                    {
                        if (offset == 0)
                        {
                            continue;
                        }

                        int parallelX = x + offset;
                        if (parallelX <= 0 || parallelX >= width - 1)
                        {
                            continue;
                        }

                        List<RandomMapPoint> loop = [];
                        int step = Math.Sign(offset);
                        for (int loopX = x; loopX != parallelX + step; loopX += step)
                        {
                            loop.Add(new RandomMapPoint(loopX, segmentStart));
                        }

                        for (int loopY = segmentStart + 1;
                            loopY <= segmentEnd;
                            loopY++)
                        {
                            loop.Add(new RandomMapPoint(parallelX, loopY));
                        }

                        for (int loopX = parallelX - step;
                            loopX != x - step;
                            loopX -= step)
                        {
                            loop.Add(new RandomMapPoint(loopX, segmentEnd));
                        }

                        if (!loop.Any(avoidedCells.Contains))
                        {
                            candidates.Add(loop);
                        }
                    }
                }

                start = Math.Max(start + 1, end + 1);
            }
        }

        random.Shuffle(candidates);
        foreach (List<RandomMapPoint> loop in candidates)
        {
            List<RandomMapPoint> changed = loop
                .Distinct()
                .Where(point => walls[point.Y, point.X])
                .ToList();
            if (changed.Count == 0)
            {
                continue;
            }

            OpenPath(walls, changed);
            InteriorGraphMetrics metrics =
                CalculateInteriorGraphMetrics(walls, roomInteriors);
            if (metrics.CycleRank > currentCycles)
            {
                return true;
            }

            foreach (RandomMapPoint point in changed)
            {
                walls[point.Y, point.X] = true;
            }
        }

        return false;
    }

    private static void EnsureRoomDoorConnections(
        bool[,] walls,
        IReadOnlyDictionary<MapRect, IReadOnlyList<RandomMapPoint>> roomDoors,
        IReadOnlyList<RandomMapPoint> mainPath,
        HashSet<RandomMapPoint> avoidedCells,
        StableRandom random)
    {
        foreach ((MapRect room, IReadOnlyList<RandomMapPoint> doors) in roomDoors)
        {
            foreach (RandomMapPoint door in doors)
            {
                RandomMapPoint outside = GetOutsideRoomPoint(room, door);
                RandomMapPoint target = FindNearest(outside, mainPath);
                List<RandomMapPoint> connection = FindOrthogonalPath(
                    walls,
                    outside,
                    target,
                    random,
                    false,
                    avoidedCells);
                walls[door.Y, door.X] = false;
                OpenPath(walls, connection);
            }
        }
    }

    private static RandomMapPoint GetOutsideRoomPoint(
        MapRect room,
        RandomMapPoint door)
    {
        if (door.Y == room.Y - 1)
        {
            return new RandomMapPoint(door.X, door.Y - 1);
        }

        if (door.Y == room.Bottom)
        {
            return new RandomMapPoint(door.X, door.Y + 1);
        }

        if (door.X == room.X - 1)
        {
            return new RandomMapPoint(door.X - 1, door.Y);
        }

        return new RandomMapPoint(door.X + 1, door.Y);
    }

    private static HashSet<RandomMapPoint> CreateSegmentAvoidedCells(
        HashSet<RandomMapPoint> avoidedCells,
        IReadOnlyList<MapRect> rooms,
        RandomMapPoint start,
        RandomMapPoint goal)
    {
        HashSet<RandomMapPoint> result = new HashSet<RandomMapPoint>(avoidedCells);
        foreach (MapRect room in rooms)
        {
            if (!room.Contains(start) && !room.Contains(goal))
            {
                continue;
            }

            foreach (RandomMapPoint point in EnumerateRoomAreaAndRing(room))
            {
                result.Remove(point);
            }
        }

        return result;
    }

    private static void EnsureMainPathRun(
        bool[,] walls,
        List<RandomMapPoint> mainPath,
        int requiredLength,
        HashSet<RandomMapPoint> avoidedCells,
        StableRandom random)
    {
        HashSet<RandomMapPoint> pathSet = mainPath.ToHashSet();
        if (FindLongestInternalPathRun(pathSet, walls.GetLength(1), walls.GetLength(0))
            >= requiredLength)
        {
            return;
        }

        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        List<List<RandomMapPoint>> internalRuns = [];

        for (int y = 1; y < height - 1; y++)
        {
            for (int startX = 1; startX + requiredLength <= width - 1; startX++)
            {
                List<RandomMapPoint> run = Enumerable.Range(startX, requiredLength)
                    .Select(x => new RandomMapPoint(x, y))
                    .ToList();
                if (!run.Any(avoidedCells.Contains))
                {
                    internalRuns.Add(run);
                }
            }
        }

        for (int x = 1; x < width - 1; x++)
        {
            for (int startY = 1; startY + requiredLength <= height - 1; startY++)
            {
                List<RandomMapPoint> run = Enumerable.Range(startY, requiredLength)
                    .Select(y => new RandomMapPoint(x, y))
                    .ToList();
                if (!run.Any(avoidedCells.Contains))
                {
                    internalRuns.Add(run);
                }
            }
        }

        if (internalRuns.Count == 0)
        {
            return;
        }

        List<RandomMapPoint> selectedRun = internalRuns
            .OrderBy(run => mainPath.Min(point => Manhattan(point, run[0])))
            .ThenBy(run => run[0].Y)
            .ThenBy(run => run[0].X)
            .First();
        RandomMapPoint connectionStart = FindNearest(selectedRun[0], mainPath);
        List<RandomMapPoint> connector = FindOrthogonalPath(
            walls,
            connectionStart,
            selectedRun[0],
            random,
            true,
            avoidedCells);
        OpenPath(walls, connector);
        OpenPath(walls, selectedRun);
        mainPath.AddRange(connector);
        mainPath.AddRange(selectedRun);
    }

    private static Dictionary<MapRect, IReadOnlyList<RandomMapPoint>> NormalizeRoomRings(
        bool[,] walls,
        IReadOnlyList<MapRect> rooms,
        IReadOnlyList<RandomMapPoint> mainPath,
        IReadOnlyDictionary<MapRect, List<RandomMapPoint>> roomCorridors)
    {
        Dictionary<MapRect, IReadOnlyList<RandomMapPoint>> selectedOpenings = [];

        foreach (MapRect room in rooms)
        {
            List<RandomMapPoint> ring = EnumerateRoomRing(room).ToList();
            List<RandomMapPoint> mainOpenings = ring
                .Where(
                    point => IsValidRoomDoor(walls, room, point)
                        && mainPath.Contains(point))
                .ToList();
            if (TryFindOppositeRoomOpenings(
                room,
                mainOpenings,
                out RandomMapPoint firstMainOpening,
                out RandomMapPoint secondMainOpening))
            {
                selectedOpenings[room] = [firstMainOpening, secondMainOpening];
                continue;
            }

            if (mainOpenings.Count == 1)
            {
                selectedOpenings[room] = [mainOpenings[0]];
                continue;
            }

            List<RandomMapPoint> candidates = ring
                .Where(
                    point => IsValidRoomDoor(walls, room, point)
                        && roomCorridors[room].Contains(point))
                .ToList();

            if (candidates.Count == 0)
            {
                candidates = ring
                    .Where(
                        point => IsValidRoomDoor(walls, room, point)
                            && !walls[point.Y, point.X])
                    .ToList();
            }

            if (candidates.Count == 0)
            {
                RandomMapPoint target = FindNearest(room.Center, mainPath);
                candidates =
                [
                    ring.Where(point => IsValidRoomDoor(walls, room, point))
                        .OrderBy(point => Manhattan(point, target))
                        .ThenBy(point => point.Y)
                        .ThenBy(point => point.X)
                        .First(),
                ];
            }

            selectedOpenings[room] = candidates.Take(1).ToList();
        }

        foreach (MapRect room in rooms)
        {
            foreach (RandomMapPoint point in EnumerateRoomRing(room))
            {
                walls[point.Y, point.X] = true;
            }
        }

        foreach (IReadOnlyList<RandomMapPoint> openings in selectedOpenings.Values)
        {
            foreach (RandomMapPoint opening in openings)
            {
                walls[opening.Y, opening.X] = false;
            }
        }

        return selectedOpenings;
    }

    private static bool TryFindOppositeRoomOpenings(
        MapRect room,
        IReadOnlyList<RandomMapPoint> candidates,
        out RandomMapPoint first,
        out RandomMapPoint second)
    {
        first = default;
        second = default;
        int farthestDistance = -1;

        for (int firstIndex = 0; firstIndex < candidates.Count - 1; firstIndex++)
        {
            for (int secondIndex = firstIndex + 1; secondIndex < candidates.Count; secondIndex++)
            {
                if (!AreOppositeRoomOpenings(
                    room,
                    candidates[firstIndex],
                    candidates[secondIndex]))
                {
                    continue;
                }

                int distance = Manhattan(candidates[firstIndex], candidates[secondIndex]);
                if (distance > farthestDistance)
                {
                    first = candidates[firstIndex];
                    second = candidates[secondIndex];
                    farthestDistance = distance;
                }
            }
        }

        return farthestDistance >= 0;
    }

    private static void SplitRegion(MapRect region, StableRandom random, List<MapRect> leaves)
    {
        bool canSplitVertical = region.Width >= 11;
        bool canSplitHorizontal = region.Height >= 11;

        if (!canSplitVertical && !canSplitHorizontal)
        {
            leaves.Add(region);
            return;
        }

        bool splitVertical;
        if (canSplitVertical && canSplitHorizontal)
        {
            if (region.Width > region.Height * 5 / 4)
            {
                splitVertical = true;
            }
            else if (region.Height > region.Width * 5 / 4)
            {
                splitVertical = false;
            }
            else
            {
                splitVertical = random.Next(2) == 0;
            }
        }
        else
        {
            splitVertical = canSplitVertical;
        }

        if (splitVertical)
        {
            int split = ChooseRegionSplit(region.Width, random);
            SplitRegion(new MapRect(region.X, region.Y, split, region.Height), random, leaves);
            SplitRegion(
                new MapRect(region.X + split, region.Y, region.Width - split, region.Height),
                random,
                leaves);
        }
        else
        {
            int split = ChooseRegionSplit(region.Height, random);
            SplitRegion(new MapRect(region.X, region.Y, region.Width, split), random, leaves);
            SplitRegion(
                new MapRect(region.X, region.Y + split, region.Width, region.Height - split),
                random,
                leaves);
        }
    }

    private static int ChooseRegionSplit(int length, StableRandom random)
    {
        int minimum = 5;
        int maximum = length - 5;
        int center = length / 2;
        int radius = Math.Max(1, length / 6);
        int lower = Math.Max(minimum, center - radius);
        int upper = Math.Min(maximum, center + radius);
        return random.NextInclusive(lower, upper);
    }

    private static MapRect SelectWeightedRoomOption(
        IReadOnlyList<MapRect> options,
        int width,
        int height,
        StableRandom random)
    {
        int totalWeight = 0;
        int[] weights = new int[options.Count];

        for (int index = 0; index < options.Count; index++)
        {
            MapRect option = options[index];
            int touchedEdges = (option.X == 1 ? 1 : 0)
                + (option.Y == 1 ? 1 : 0)
                + (option.Right == width - 1 ? 1 : 0)
                + (option.Bottom == height - 1 ? 1 : 0);
            int weight = 1 + touchedEdges * 3;
            weights[index] = weight;
            totalWeight += weight;
        }

        int selection = random.Next(totalWeight);
        for (int index = 0; index < options.Count; index++)
        {
            if (selection < weights[index])
            {
                return options[index];
            }

            selection -= weights[index];
        }

        return options[^1];
    }

    private static bool HasRoomRingOnActualEdge(
        MapRect room,
        int width,
        int height)
    {
        return room.X == 2
            || room.Y == 2
            || room.Right == width - 2
            || room.Bottom == height - 2;
    }

    private static List<RandomMapPoint> BuildMarkerRoute(
        int width,
        int height,
        HashSet<RandomMapPoint> markers,
        StableRandom random)
    {
        if (markers.Count == 0)
        {
            if (random.Next(2) == 0)
            {
                return
                [
                    new RandomMapPoint(1, 1),
                    new RandomMapPoint(width - 2, height - 2),
                ];
            }

            return
            [
                new RandomMapPoint(width - 2, 1),
                new RandomMapPoint(1, height - 2),
            ];
        }

        if (markers.Count == 1)
        {
            RandomMapPoint marker = markers.First();
            RandomMapPoint[] corners =
            [
                new RandomMapPoint(1, 1),
                new RandomMapPoint(width - 2, 1),
                new RandomMapPoint(1, height - 2),
                new RandomMapPoint(width - 2, height - 2),
            ];
            RandomMapPoint farthest = corners
                .OrderByDescending(point => Manhattan(marker, point))
                .ThenBy(point => point.Y)
                .ThenBy(point => point.X)
                .First();
            return [marker, farthest];
        }

        List<RandomMapPoint> points = markers
            .OrderBy(point => point.Y)
            .ThenBy(point => point.X)
            .ToList();
        RandomMapPoint first = points[0];
        RandomMapPoint last = points[1];
        int farthestDistance = -1;

        for (int firstIndex = 0; firstIndex < points.Count - 1; firstIndex++)
        {
            for (int secondIndex = firstIndex + 1; secondIndex < points.Count; secondIndex++)
            {
                int distance = Manhattan(points[firstIndex], points[secondIndex]);
                if (distance > farthestDistance)
                {
                    farthestDistance = distance;
                    first = points[firstIndex];
                    last = points[secondIndex];
                }
            }
        }

        List<RandomMapPoint> route = [first, last];
        points.Remove(first);
        points.Remove(last);

        foreach (RandomMapPoint point in points)
        {
            int bestPosition = 1;
            int bestIncrease = int.MaxValue;

            for (int position = 1; position < route.Count; position++)
            {
                int increase = Manhattan(route[position - 1], point)
                    + Manhattan(point, route[position])
                    - Manhattan(route[position - 1], route[position]);

                if (increase < bestIncrease)
                {
                    bestIncrease = increase;
                    bestPosition = position;
                }
            }

            route.Insert(bestPosition, point);
        }

        bool improved = true;
        while (improved)
        {
            improved = false;
            for (int firstIndex = 1; firstIndex < route.Count - 2; firstIndex++)
            {
                for (int secondIndex = firstIndex + 1; secondIndex < route.Count - 1; secondIndex++)
                {
                    int oldDistance = Manhattan(route[firstIndex - 1], route[firstIndex])
                        + Manhattan(route[secondIndex], route[secondIndex + 1]);
                    int newDistance = Manhattan(route[firstIndex - 1], route[secondIndex])
                        + Manhattan(route[firstIndex], route[secondIndex + 1]);

                    if (newDistance < oldDistance)
                    {
                        route.Reverse(firstIndex, secondIndex - firstIndex + 1);
                        improved = true;
                    }
                }
            }
        }

        return route;
    }

    private static List<RandomMapPoint> FindOrthogonalPath(
        bool[,] walls,
        RandomMapPoint start,
        RandomMapPoint goal,
        StableRandom random,
        bool preferLongRuns,
        HashSet<RandomMapPoint>? avoidedCells)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        if (!IsInterior(start.X, start.Y, width, height)
            || !IsInterior(goal.X, goal.Y, width, height))
        {
            return [];
        }

        int[,,] distances = new int[height, width, 5];
        PathPredecessor[,,] predecessors = new PathPredecessor[height, width, 5];
        byte[,] jitter = new byte[height, width];

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                jitter[y, x] = (byte)random.Next(3);
                for (int direction = 0; direction < 5; direction++)
                {
                    distances[y, x, direction] = int.MaxValue;
                }
            }
        }

        PriorityQueue<PathState, int> queue = new PriorityQueue<PathState, int>();
        distances[start.Y, start.X, 4] = 0;
        queue.Enqueue(new PathState(start.X, start.Y, 4), Manhattan(start, goal) * 10);
        PathState? completed = null;

        while (queue.TryDequeue(out PathState current, out int priority))
        {
            int currentDistance = distances[current.Y, current.X, current.Direction];
            int expectedPriority = currentDistance
                + Manhattan(new RandomMapPoint(current.X, current.Y), goal) * 10;
            if (priority != expectedPriority)
            {
                continue;
            }

            if (current.X == goal.X && current.Y == goal.Y)
            {
                completed = current;
                break;
            }

            for (int directionIndex = 0; directionIndex < Directions.Length; directionIndex++)
            {
                RandomMapPoint direction = Directions[directionIndex];
                int nextX = current.X + direction.X;
                int nextY = current.Y + direction.Y;

                if (!IsInterior(nextX, nextY, width, height))
                {
                    continue;
                }

                if (avoidedCells is not null
                    && avoidedCells.Contains(new RandomMapPoint(nextX, nextY))
                    && (nextX != goal.X || nextY != goal.Y))
                {
                    continue;
                }

                int turnCost = current.Direction == 4 || current.Direction == directionIndex
                    ? 0
                    : preferLongRuns ? 45 : 24;
                int nextDistance = currentDistance
                    + 10
                    + turnCost
                    + jitter[nextY, nextX];
                if (nextDistance >= distances[nextY, nextX, directionIndex])
                {
                    continue;
                }

                distances[nextY, nextX, directionIndex] = nextDistance;
                predecessors[nextY, nextX, directionIndex] =
                    new PathPredecessor(current.X, current.Y, current.Direction, true);
                int estimate = nextDistance
                    + Manhattan(new RandomMapPoint(nextX, nextY), goal) * 10;
                queue.Enqueue(new PathState(nextX, nextY, directionIndex), estimate);
            }
        }

        if (completed is null)
        {
            return [start];
        }

        List<RandomMapPoint> reversed = [];
        PathState cursor = completed.Value;
        reversed.Add(new RandomMapPoint(cursor.X, cursor.Y));

        while (cursor.X != start.X || cursor.Y != start.Y || cursor.Direction != 4)
        {
            PathPredecessor predecessor =
                predecessors[cursor.Y, cursor.X, cursor.Direction];
            if (!predecessor.HasValue)
            {
                break;
            }

            cursor = new PathState(
                predecessor.X,
                predecessor.Y,
                predecessor.Direction);
            reversed.Add(new RandomMapPoint(cursor.X, cursor.Y));
        }

        reversed.Reverse();
        return reversed;
    }

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

    private static bool ConnectEmptyComponents(
        bool[,] walls,
        HashSet<RandomMapPoint>? forbiddenWalls = null)
    {
        int maximumIterations = GetPlayableArea(walls);

        for (int iteration = 0; iteration < maximumIterations; iteration++)
        {
            int[,] components = LabelEmptyComponents(walls, out int componentCount);
            if (componentCount <= 1)
            {
                return componentCount == 1;
            }

            if (!OpenCheapestConnection(
                walls,
                components,
                0,
                1,
                forbiddenWalls))
            {
                return false;
            }
        }

        return false;
    }

    private static int[,] LabelEmptyComponents(bool[,] walls, out int componentCount)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int[,] components = new int[height, width];

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                components[y, x] = -1;
            }
        }

        componentCount = 0;
        Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (walls[y, x] || components[y, x] >= 0)
                {
                    continue;
                }

                components[y, x] = componentCount;
                queue.Enqueue(new RandomMapPoint(x, y));

                while (queue.Count > 0)
                {
                    RandomMapPoint current = queue.Dequeue();
                    foreach (RandomMapPoint direction in Directions)
                    {
                        int nextX = current.X + direction.X;
                        int nextY = current.Y + direction.Y;
                        if (!IsInterior(nextX, nextY, width, height))
                        {
                            continue;
                        }

                        if (walls[nextY, nextX] || components[nextY, nextX] >= 0)
                        {
                            continue;
                        }

                        components[nextY, nextX] = componentCount;
                        queue.Enqueue(new RandomMapPoint(nextX, nextY));
                    }
                }

                componentCount++;
            }
        }

        return components;
    }

    private static bool OpenCheapestConnection(
        bool[,] walls,
        int[,] components,
        int sourceComponent,
        int targetComponent,
        HashSet<RandomMapPoint>? forbiddenWalls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int[,] distances = new int[height, width];
        RandomMapPoint?[,] predecessors = new RandomMapPoint?[height, width];
        LinkedList<RandomMapPoint> deque = new LinkedList<RandomMapPoint>();

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                distances[y, x] = int.MaxValue;
                if (components[y, x] == sourceComponent)
                {
                    distances[y, x] = 0;
                    deque.AddLast(new RandomMapPoint(x, y));
                }
            }
        }

        RandomMapPoint? destination = null;

        while (deque.Count > 0)
        {
            RandomMapPoint current = deque.First!.Value;
            deque.RemoveFirst();

            if (components[current.Y, current.X] == targetComponent)
            {
                destination = current;
                break;
            }

            foreach (RandomMapPoint direction in Directions)
            {
                int nextX = current.X + direction.X;
                int nextY = current.Y + direction.Y;
                if (!IsInterior(nextX, nextY, width, height))
                {
                    continue;
                }

                RandomMapPoint next = new RandomMapPoint(nextX, nextY);
                if (walls[nextY, nextX]
                    && forbiddenWalls is not null
                    && forbiddenWalls.Contains(next))
                {
                    continue;
                }

                int cost = walls[nextY, nextX] ? 1 : 0;
                int nextDistance = distances[current.Y, current.X] + cost;
                if (nextDistance >= distances[nextY, nextX])
                {
                    continue;
                }

                distances[nextY, nextX] = nextDistance;
                predecessors[nextY, nextX] = current;
                if (cost == 0)
                {
                    deque.AddFirst(next);
                }
                else
                {
                    deque.AddLast(next);
                }
            }
        }

        if (destination is null)
        {
            return false;
        }

        RandomMapPoint cursor = destination.Value;
        while (components[cursor.Y, cursor.X] != sourceComponent)
        {
            walls[cursor.Y, cursor.X] = false;
            RandomMapPoint? predecessor = predecessors[cursor.Y, cursor.X];
            if (predecessor is null)
            {
                return false;
            }

            cursor = predecessor.Value;
        }

        return true;
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

    private static bool HasSingleEmptyComponent(bool[,] walls)
    {
        LabelEmptyComponents(walls, out int componentCount);
        return componentCount == 1;
    }

    private static bool HasSealedVirtualBoundary(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        for (int x = 0; x < width; x++)
        {
            if (!walls[0, x] || !walls[height - 1, x])
            {
                return false;
            }
        }

        for (int y = 1; y < height - 1; y++)
        {
            if (!walls[y, 0] || !walls[y, width - 1])
            {
                return false;
            }
        }

        return true;
    }

    private static bool HasEmptyWindow(bool[,] walls, int windowSize)
    {
        return TryFindEmptyWindow(walls, windowSize, out MapRect unused);
    }

    private static bool HasWallBlock(bool[,] walls)
    {
        return TryFindWallBlock(walls, out MapRect unused);
    }

    private static bool TryFindWallBlock(bool[,] walls, out MapRect block)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        for (int y = 0; y < height - 1; y++)
        {
            for (int x = 0; x < width - 1; x++)
            {
                if (IsWallBlock(walls, x, y))
                {
                    block = new MapRect(x, y, 2, 2);
                    return true;
                }
            }
        }

        block = default;
        return false;
    }

    private static bool WouldCreateWallBlock(bool[,] walls, int changedX, int changedY)
    {
        GetContainingWallBlockBounds(
            walls,
            changedX,
            changedY,
            out int minimumX,
            out int maximumX,
            out int minimumY,
            out int maximumY);

        for (int startY = minimumY; startY <= maximumY; startY++)
        {
            for (int startX = minimumX; startX <= maximumX; startX++)
            {
                bool allWalls = true;
                for (int y = startY; y < startY + 2 && allWalls; y++)
                {
                    for (int x = startX; x < startX + 2; x++)
                    {
                        if ((x != changedX || y != changedY) && !walls[y, x])
                        {
                            allWalls = false;
                            break;
                        }
                    }
                }

                if (allWalls)
                {
                    return true;
                }
            }
        }

        return false;
    }

    private static int CountWallBlocksContaining(bool[,] walls, int x, int y)
    {
        GetContainingWallBlockBounds(
            walls,
            x,
            y,
            out int minimumX,
            out int maximumX,
            out int minimumY,
            out int maximumY);
        int count = 0;

        for (int startY = minimumY; startY <= maximumY; startY++)
        {
            for (int startX = minimumX; startX <= maximumX; startX++)
            {
                if (IsWallBlock(walls, startX, startY))
                {
                    count++;
                }
            }
        }

        return count;
    }

    private static int CountWallBlockThreats(bool[,] walls, int x, int y)
    {
        GetContainingWallBlockBounds(
            walls,
            x,
            y,
            out int minimumX,
            out int maximumX,
            out int minimumY,
            out int maximumY);
        int count = 0;

        for (int startY = minimumY; startY <= maximumY; startY++)
        {
            for (int startX = minimumX; startX <= maximumX; startX++)
            {
                int wallCount = 0;
                for (int blockY = startY; blockY < startY + 2; blockY++)
                {
                    for (int blockX = startX; blockX < startX + 2; blockX++)
                    {
                        if ((blockX == x && blockY == y)
                            || walls[blockY, blockX])
                        {
                            wallCount++;
                        }
                    }
                }

                if (wallCount == 3)
                {
                    count++;
                }
            }
        }

        return count;
    }

    private static bool IsWallBlock(bool[,] walls, int x, int y)
    {
        return walls[y, x]
            && walls[y, x + 1]
            && walls[y + 1, x]
            && walls[y + 1, x + 1];
    }

    private static void GetContainingWallBlockBounds(
        bool[,] walls,
        int x,
        int y,
        out int minimumX,
        out int maximumX,
        out int minimumY,
        out int maximumY)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        minimumX = Math.Max(0, x - 1);
        maximumX = Math.Min(x, width - 2);
        minimumY = Math.Max(0, y - 1);
        maximumY = Math.Min(y, height - 2);
    }

    private static bool TryFindEmptyWindow(bool[,] walls, int windowSize, out MapRect window)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        for (int startY = 1; startY + windowSize <= height - 1; startY++)
        {
            for (int startX = 1; startX + windowSize <= width - 1; startX++)
            {
                bool allEmpty = true;

                for (int y = startY; y < startY + windowSize && allEmpty; y++)
                {
                    for (int x = startX; x < startX + windowSize; x++)
                    {
                        if (walls[y, x])
                        {
                            allEmpty = false;
                            break;
                        }
                    }
                }

                if (allEmpty)
                {
                    window = new MapRect(startX, startY, windowSize, windowSize);
                    return true;
                }
            }
        }

        window = default;
        return false;
    }

    private static bool WouldCreateEmptyWindow(
        bool[,] walls,
        int changedX,
        int changedY,
        int windowSize)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int minimumX = Math.Max(1, changedX - windowSize + 1);
        int maximumX = Math.Min(changedX, width - windowSize - 1);
        int minimumY = Math.Max(1, changedY - windowSize + 1);
        int maximumY = Math.Min(changedY, height - windowSize - 1);

        for (int startY = minimumY; startY <= maximumY; startY++)
        {
            for (int startX = minimumX; startX <= maximumX; startX++)
            {
                bool allEmpty = true;
                for (int y = startY; y < startY + windowSize && allEmpty; y++)
                {
                    for (int x = startX; x < startX + windowSize; x++)
                    {
                        if (walls[y, x])
                        {
                            allEmpty = false;
                            break;
                        }
                    }
                }

                if (allEmpty)
                {
                    return true;
                }
            }
        }

        return false;
    }

    private static HashSet<RandomMapPoint> CollectRoomRingWalls(GenerationCandidate candidate)
    {
        HashSet<RandomMapPoint> protectedWalls = [];
        foreach (MapRect room in candidate.Rooms)
        {
            foreach (RandomMapPoint point in EnumerateRoomRing(room))
            {
                if (candidate.Walls[point.Y, point.X])
                {
                    protectedWalls.Add(point);
                }
            }
        }

        protectedWalls.UnionWith(candidate.RoomNotches);
        return protectedWalls;
    }

    private static Dictionary<RandomMapPoint, int> LabelInteriorGraphComponents(
        bool[,] walls,
        HashSet<RandomMapPoint> excluded)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        Dictionary<RandomMapPoint, int> components = [];
        Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();
        int component = 0;

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                RandomMapPoint root = new RandomMapPoint(x, y);
                if (walls[y, x]
                    || excluded.Contains(root)
                    || components.ContainsKey(root))
                {
                    continue;
                }

                components[root] = component;
                queue.Enqueue(root);
                while (queue.Count > 0)
                {
                    RandomMapPoint current = queue.Dequeue();
                    foreach (RandomMapPoint direction in Directions)
                    {
                        RandomMapPoint next = new RandomMapPoint(
                            current.X + direction.X,
                            current.Y + direction.Y);
                        if (IsInterior(next.X, next.Y, width, height)
                            && !walls[next.Y, next.X]
                            && !excluded.Contains(next)
                            && components.TryAdd(next, component))
                        {
                            queue.Enqueue(next);
                        }
                    }
                }

                component++;
            }
        }

        return components;
    }

    private static InteriorGraphMetrics CalculateInteriorGraphMetrics(
        bool[,] walls,
        HashSet<RandomMapPoint>? excluded)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        HashSet<RandomMapPoint> vertices = [];

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                RandomMapPoint point = new RandomMapPoint(x, y);
                if (!walls[y, x]
                    && (excluded is null || !excluded.Contains(point)))
                {
                    vertices.Add(point);
                }
            }
        }

        if (vertices.Count == 0)
        {
            return new InteriorGraphMetrics(0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        int edges = 0;
        int junctions = 0;
        int deadEnds = 0;
        int denseVertices = 0;
        Dictionary<RandomMapPoint, int> degrees = [];

        foreach (RandomMapPoint point in vertices)
        {
            int degree = 0;
            foreach (RandomMapPoint direction in Directions)
            {
                if (vertices.Contains(
                    new RandomMapPoint(
                        point.X + direction.X,
                        point.Y + direction.Y)))
                {
                    degree++;
                }
            }

            degrees[point] = degree;
            if (degree >= 3)
            {
                junctions++;
            }

            if (degree <= 1)
            {
                deadEnds++;
            }

            if (degree == 4)
            {
                denseVertices++;
            }

            if (vertices.Contains(new RandomMapPoint(point.X + 1, point.Y)))
            {
                edges++;
            }

            if (vertices.Contains(new RandomMapPoint(point.X, point.Y + 1)))
            {
                edges++;
            }
        }

        int components = 0;
        int largestComponent = 0;
        HashSet<RandomMapPoint> visited = [];
        Queue<RandomMapPoint> componentQueue = new Queue<RandomMapPoint>();

        foreach (RandomMapPoint root in vertices)
        {
            if (!visited.Add(root))
            {
                continue;
            }

            components++;
            int componentSize = 0;
            componentQueue.Enqueue(root);
            while (componentQueue.Count > 0)
            {
                RandomMapPoint current = componentQueue.Dequeue();
                componentSize++;
                foreach (RandomMapPoint direction in Directions)
                {
                    RandomMapPoint next = new RandomMapPoint(
                        current.X + direction.X,
                        current.Y + direction.Y);
                    if (vertices.Contains(next) && visited.Add(next))
                    {
                        componentQueue.Enqueue(next);
                    }
                }
            }

            largestComponent = Math.Max(largestComponent, componentSize);
        }

        Dictionary<RandomMapPoint, int> remainingDegrees =
            new Dictionary<RandomMapPoint, int>(degrees);
        Queue<RandomMapPoint> peelQueue = new Queue<RandomMapPoint>();
        HashSet<RandomMapPoint> removed = [];
        foreach ((RandomMapPoint point, int degree) in remainingDegrees)
        {
            if (degree < 2)
            {
                peelQueue.Enqueue(point);
            }
        }

        while (peelQueue.Count > 0)
        {
            RandomMapPoint current = peelQueue.Dequeue();
            if (!removed.Add(current))
            {
                continue;
            }

            foreach (RandomMapPoint direction in Directions)
            {
                RandomMapPoint next = new RandomMapPoint(
                    current.X + direction.X,
                    current.Y + direction.Y);
                if (!remainingDegrees.ContainsKey(next) || removed.Contains(next))
                {
                    continue;
                }

                remainingDegrees[next]--;
                if (remainingDegrees[next] == 1)
                {
                    peelQueue.Enqueue(next);
                }
            }
        }

        int coreVertices = vertices.Count - removed.Count;
        int cycleRank = Math.Max(0, edges - vertices.Count + components);
        return new InteriorGraphMetrics(
            vertices.Count,
            edges,
            components,
            largestComponent,
            cycleRank,
            coreVertices,
            junctions,
            deadEnds,
            denseVertices);
    }

    private static bool HasValidLatticeStyle(GenerationCandidate candidate)
    {
        bool[,] walls = candidate.Walls;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int branches = 0;

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (!walls[y, x] && CountInteriorOpenNeighbors(walls, x, y) >= 3)
                {
                    branches++;
                }
            }
        }

        int requiredBranches = Math.Max(2, (candidate.LatticeNodeCount + 11) / 12);
        if (branches < requiredBranches)
        {
            return false;
        }

        InteriorGraphMetrics metrics = CalculateInteriorGraphMetrics(walls, null);
        int requiredCycles = candidate.LatticeNodeCount <= 9
            ? 1
            : Math.Max(2, (candidate.LatticeNodeCount + 17) / 18);
        double minimumCoreRatio = candidate.LatticeNodeCount <= 9 ? 0.15 : 0.22;
        if (metrics.CycleRank < requiredCycles
            || metrics.CoreVertices < Math.Ceiling(metrics.Vertices * minimumCoreRatio)
            || metrics.LargestComponent * 100 < metrics.Vertices * 85
            || metrics.DenseVertices > Math.Max(2, metrics.Vertices * 15 / 100)
            || metrics.DeadEnds * 100 > metrics.Vertices * 42)
        {
            return false;
        }

        List<int> corridorLengths = FindCompressedCorridorLengths(walls);
        if (corridorLengths.Count == 0)
        {
            return false;
        }

        corridorLengths.Sort();
        int percentileIndex = Math.Max(
            0,
            (int)Math.Ceiling(corridorLengths.Count * 0.90) - 1);
        int percentile90 = corridorLengths[percentileIndex];
        int maximumCorridor = Math.Clamp(
            (int)Math.Ceiling(Math.Min(width - 2, height - 2) / 4.0),
            4,
            8);
        return percentile90 <= maximumCorridor;
    }

    private static List<int> FindCompressedCorridorLengths(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        HashSet<long> visitedEdges = [];
        List<int> lengths = [];

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (walls[y, x] || CountInteriorOpenNeighbors(walls, x, y) == 2)
                {
                    continue;
                }

                RandomMapPoint start = new RandomMapPoint(x, y);
                foreach (RandomMapPoint neighbor in EnumerateInteriorOpenNeighbors(walls, start))
                {
                    long firstEdge = EncodeEdge(start, neighbor, width);
                    if (!visitedEdges.Add(firstEdge))
                    {
                        continue;
                    }

                    int length = 1;
                    RandomMapPoint previous = start;
                    RandomMapPoint current = neighbor;

                    while (CountInteriorOpenNeighbors(walls, current.X, current.Y) == 2)
                    {
                        RandomMapPoint next = EnumerateInteriorOpenNeighbors(walls, current)
                            .First(point => point != previous);
                        visitedEdges.Add(EncodeEdge(current, next, width));
                        previous = current;
                        current = next;
                        length++;
                    }

                    lengths.Add(length);
                }
            }
        }

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (walls[y, x])
                {
                    continue;
                }

                RandomMapPoint start = new RandomMapPoint(x, y);
                foreach (RandomMapPoint neighbor in EnumerateInteriorOpenNeighbors(walls, start))
                {
                    long firstEdge = EncodeEdge(start, neighbor, width);
                    if (!visitedEdges.Add(firstEdge))
                    {
                        continue;
                    }

                    int length = 1;
                    RandomMapPoint previous = start;
                    RandomMapPoint current = neighbor;

                    while (current != start)
                    {
                        List<RandomMapPoint> nextCandidates =
                            EnumerateInteriorOpenNeighbors(walls, current)
                                .Where(point => point != previous)
                                .ToList();
                        if (nextCandidates.Count == 0)
                        {
                            break;
                        }

                        RandomMapPoint next = nextCandidates[0];
                        long edge = EncodeEdge(current, next, width);
                        if (!visitedEdges.Add(edge))
                        {
                            break;
                        }

                        previous = current;
                        current = next;
                        length++;
                    }

                    lengths.Add(length);
                }
            }
        }

        return lengths;
    }

    private static int CountInteriorOpenNeighbors(bool[,] walls, int x, int y)
    {
        int count = 0;
        foreach (RandomMapPoint unused in EnumerateInteriorOpenNeighbors(
            walls,
            new RandomMapPoint(x, y)))
        {
            count++;
        }

        return count;
    }

    private static IEnumerable<RandomMapPoint> EnumerateInteriorOpenNeighbors(
        bool[,] walls,
        RandomMapPoint point)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        foreach (RandomMapPoint direction in Directions)
        {
            int x = point.X + direction.X;
            int y = point.Y + direction.Y;
            if (IsInterior(x, y, width, height) && !walls[y, x])
            {
                yield return new RandomMapPoint(x, y);
            }
        }
    }

    private static long EncodeEdge(
        RandomMapPoint first,
        RandomMapPoint second,
        int width)
    {
        int firstIndex = first.Y * width + first.X;
        int secondIndex = second.Y * width + second.X;
        int minimum = Math.Min(firstIndex, secondIndex);
        int maximum = Math.Max(firstIndex, secondIndex);
        return ((long)minimum << 32) | unchecked((uint)maximum);
    }

    private static bool HasValidRoomStyle(
        GenerationCandidate candidate,
        HashSet<RandomMapPoint> markers,
        out string failure)
    {
        failure = string.Empty;
        bool[,] walls = candidate.Walls;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int playableHeight = height - 2;
        int playableWidth = width - 2;
        int playableArea = playableWidth * playableHeight;
        int requiredRun = Math.Max(4, Math.Max(playableWidth, playableHeight) / 4);

        if (!TryRebuildMainPath(candidate, markers, requiredRun))
        {
            failure = "noMainTrunk";
            return false;
        }

        if (candidate.MainPath.Any(point => walls[point.Y, point.X]))
        {
            failure = "noMainTrunk";
            return false;
        }

        if (!IsPointSetConnected(candidate.MainPath))
        {
            failure = "noMainTrunk";
            return false;
        }

        if (!markers.All(candidate.MainPath.Contains))
        {
            failure = "noMainTrunk";
            return false;
        }

        if (FindLongestInternalPathRun(candidate.MainPath, width, height) < requiredRun)
        {
            failure = "noMainTrunk";
            return false;
        }

        if (candidate.Rooms.Count == 0)
        {
            failure = "noRoomLayout";
            return false;
        }

        int minimumRooms = playableArea < 120
            ? 1
            : playableArea < 225
                ? 2
                : Math.Clamp(playableArea / 120, 3, 10);
        if (candidate.Rooms.Count < minimumRooms)
        {
            failure = "noRoomCount";
            return false;
        }

        HashSet<int> occupiedQuadrants = candidate.Rooms
            .Select(
                room => (room.Center.Y >= height / 2 ? 2 : 0)
                    + (room.Center.X >= width / 2 ? 1 : 0))
            .ToHashSet();
        int requiredQuadrants = candidate.Rooms.Count >= 5
            && playableWidth >= 13
            && playableHeight >= 13
                ? 3
                : Math.Min(2, candidate.Rooms.Count);
        if (occupiedQuadrants.Count < requiredQuadrants)
        {
            failure = "noRoomDistribution";
            return false;
        }

        if (candidate.Rooms.Count >= 3)
        {
            int longestAxisSpan = playableWidth >= playableHeight
                ? candidate.Rooms.Max(room => room.Center.X)
                    - candidate.Rooms.Min(room => room.Center.X)
                : candidate.Rooms.Max(room => room.Center.Y)
                    - candidate.Rooms.Min(room => room.Center.Y);
            int longestInteriorAxis = Math.Max(playableWidth, playableHeight);
            if (longestAxisSpan * 5 < longestInteriorAxis * 2)
            {
                failure = "noRoomDistribution";
                return false;
            }
        }

        HashSet<RandomMapPoint> roomInteriors = [];
        foreach (MapRect room in candidate.Rooms)
        {
            if (room.Width * room.Height > 20
                || !IsRoomInteriorConnected(walls, room))
            {
                failure = "noRoomShape";
                return false;
            }

            int roomNotches = 0;
            for (int y = room.Y; y < room.Bottom; y++)
            {
                for (int x = room.X; x < room.Right; x++)
                {
                    RandomMapPoint point = new RandomMapPoint(x, y);
                    roomInteriors.Add(point);
                    if (candidate.RoomNotches.Contains(point))
                    {
                        roomNotches++;
                    }
                }
            }

            if (roomNotches > 1)
            {
                failure = "noRoomShape";
                return false;
            }

            List<RandomMapPoint> ring = EnumerateRoomRing(room).ToList();
            List<RandomMapPoint> openings = ring
                .Where(point => !walls[point.Y, point.X])
                .ToList();
            int wallCount = ring.Count - openings.Count;
            IReadOnlyList<RandomMapPoint> expectedOpenings = candidate.RoomDoors[room];

            if (wallCount * 4 < ring.Count * 3
                || expectedOpenings.Count < 1
                || openings.Count != expectedOpenings.Count
                || (openings.Count == 2
                    && !AreOppositeRoomOpenings(room, openings[0], openings[1])))
            {
                failure = "noRoomDoors";
                return false;
            }

        }

        if (playableArea >= 120 && candidate.RoomNotches.Count == 0)
        {
            failure = "noRoomShape";
            return false;
        }

        if (candidate.RoomNotches.Any(
            point => !walls[point.Y, point.X]
                || !candidate.Rooms.Any(room => room.Contains(point))))
        {
            failure = "noRoomShape";
            return false;
        }

        InteriorGraphMetrics corridorMetrics =
            CalculateInteriorGraphMetrics(walls, roomInteriors);
        bool requiresCorridorCycle = playableArea >= 120
            && Math.Min(playableWidth, playableHeight) >= 9;
        if (requiresCorridorCycle && corridorMetrics.CycleRank < 1)
        {
            failure = "noRoomCycles";
            return false;
        }

        int requiredJunctions = Math.Max(1, candidate.Rooms.Count / 2);
        if (playableArea >= 120
            && ((requiresCorridorCycle && corridorMetrics.CoreVertices < 4)
                || corridorMetrics.Junctions < requiredJunctions))
        {
            failure = "noRoomRoutes";
            return false;
        }

        int openCells = playableArea - CountWalls(walls);
        double maximumDenseRatio = Math.Max(
            0.125,
            openCells / (playableArea * 4.0));
        int maximumDenseVertices = Math.Max(
            3,
            (int)Math.Floor(corridorMetrics.Vertices * maximumDenseRatio));
        if (corridorMetrics.DenseVertices > maximumDenseVertices)
        {
            failure = "noRoomRoutes";
            return false;
        }

        return true;
    }

    private static bool TryRebuildMainPath(
        GenerationCandidate candidate,
        HashSet<RandomMapPoint> markers,
        int requiredRun)
    {
        bool[,] walls = candidate.Walls;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        List<List<RandomMapPoint>> runs = [];

        for (int y = 1; y < height - 1; y++)
        {
            for (int startX = 1; startX + requiredRun <= width - 1; startX++)
            {
                List<RandomMapPoint> run = Enumerable.Range(startX, requiredRun)
                    .Select(x => new RandomMapPoint(x, y))
                    .ToList();
                if (run.All(point => !walls[point.Y, point.X])
                    && !candidate.Rooms.Any(
                        room => run.All(room.Contains)))
                {
                    runs.Add(run);
                }
            }
        }

        for (int x = 1; x < width - 1; x++)
        {
            for (int startY = 1; startY + requiredRun <= height - 1; startY++)
            {
                List<RandomMapPoint> run = Enumerable.Range(startY, requiredRun)
                    .Select(y => new RandomMapPoint(x, y))
                    .ToList();
                if (run.All(point => !walls[point.Y, point.X])
                    && !candidate.Rooms.Any(
                        room => run.All(room.Contains)))
                {
                    runs.Add(run);
                }
            }
        }

        if (runs.Count == 0)
        {
            return false;
        }

        List<RandomMapPoint> selectedRun = runs
            .OrderBy(run => markers.Count == 0
                ? 0
                : markers.Min(marker => Manhattan(marker, run[0])))
            .ThenBy(run => run[0].Y)
            .ThenBy(run => run[0].X)
            .First();
        HashSet<RandomMapPoint> rebuilt = selectedRun.ToHashSet();
        RandomMapPoint root = selectedRun[0];

        foreach (RandomMapPoint marker in markers)
        {
            List<RandomMapPoint> path = FindEmptyPath(walls, marker, root);
            if (path.Count == 0)
            {
                return false;
            }

            rebuilt.UnionWith(path);
        }

        candidate.MainPath.Clear();
        candidate.MainPath.UnionWith(rebuilt);
        return true;
    }

    private static List<RandomMapPoint> FindEmptyPath(
        bool[,] walls,
        RandomMapPoint start,
        RandomMapPoint goal)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        bool[,] visited = new bool[height, width];
        RandomMapPoint?[,] predecessors = new RandomMapPoint?[height, width];
        Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();
        visited[start.Y, start.X] = true;
        queue.Enqueue(start);

        while (queue.Count > 0)
        {
            RandomMapPoint current = queue.Dequeue();
            if (current == goal)
            {
                List<RandomMapPoint> path = [current];
                while (current != start)
                {
                    RandomMapPoint? predecessor =
                        predecessors[current.Y, current.X];
                    if (predecessor is null)
                    {
                        return [];
                    }

                    current = predecessor.Value;
                    path.Add(current);
                }

                path.Reverse();
                return path;
            }

            foreach (RandomMapPoint direction in Directions)
            {
                int x = current.X + direction.X;
                int y = current.Y + direction.Y;
                if (IsInterior(x, y, width, height)
                    && !walls[y, x]
                    && !visited[y, x])
                {
                    visited[y, x] = true;
                    predecessors[y, x] = current;
                    queue.Enqueue(new RandomMapPoint(x, y));
                }
            }
        }

        return [];
    }

    private static bool IsPointSetConnected(HashSet<RandomMapPoint> points)
    {
        if (points.Count == 0)
        {
            return false;
        }

        HashSet<RandomMapPoint> visited = [];
        Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();
        RandomMapPoint first = points.First();
        visited.Add(first);
        queue.Enqueue(first);

        while (queue.Count > 0)
        {
            RandomMapPoint current = queue.Dequeue();
            foreach (RandomMapPoint direction in Directions)
            {
                RandomMapPoint next = new RandomMapPoint(
                    current.X + direction.X,
                    current.Y + direction.Y);
                if (points.Contains(next) && visited.Add(next))
                {
                    queue.Enqueue(next);
                }
            }
        }

        return visited.Count == points.Count;
    }

    private static int FindLongestInternalPathRun(
        HashSet<RandomMapPoint> path,
        int width,
        int height)
    {
        int longest = 0;

        foreach (RandomMapPoint point in path)
        {
            if (!IsInterior(point.X, point.Y, width, height))
            {
                continue;
            }

            if (!path.Contains(new RandomMapPoint(point.X - 1, point.Y))
                || point.X == 1)
            {
                int length = 1;
                while (point.X + length < width - 1
                    && path.Contains(new RandomMapPoint(point.X + length, point.Y)))
                {
                    length++;
                }

                longest = Math.Max(longest, length);
            }

            if (!path.Contains(new RandomMapPoint(point.X, point.Y - 1))
                || point.Y == 1)
            {
                int length = 1;
                while (point.Y + length < height - 1
                    && path.Contains(new RandomMapPoint(point.X, point.Y + length)))
                {
                    length++;
                }

                longest = Math.Max(longest, length);
            }
        }

        return longest;
    }

    private static IEnumerable<RandomMapPoint> EnumerateRoomRing(MapRect room)
    {
        int top = room.Y - 1;
        int bottom = room.Bottom;
        int left = room.X - 1;
        int right = room.Right;

        for (int x = left; x <= right; x++)
        {
            yield return new RandomMapPoint(x, top);
            yield return new RandomMapPoint(x, bottom);
        }

        for (int y = room.Y; y < room.Bottom; y++)
        {
            yield return new RandomMapPoint(left, y);
            yield return new RandomMapPoint(right, y);
        }
    }

    private static IEnumerable<RandomMapPoint> EnumerateRoomAreaAndRing(MapRect room)
    {
        for (int y = room.Y - 1; y <= room.Bottom; y++)
        {
            for (int x = room.X - 1; x <= room.Right; x++)
            {
                yield return new RandomMapPoint(x, y);
            }
        }
    }

    private static bool IsOnRoomRing(MapRect room, RandomMapPoint point)
    {
        bool withinHorizontalSpan = point.X >= room.X - 1 && point.X <= room.Right;
        bool withinVerticalSpan = point.Y >= room.Y - 1 && point.Y <= room.Bottom;
        return (withinHorizontalSpan
                && (point.Y == room.Y - 1 || point.Y == room.Bottom))
            || (withinVerticalSpan
                && (point.X == room.X - 1 || point.X == room.Right));
    }

    private static bool IsValidRoomDoor(
        bool[,] walls,
        MapRect room,
        RandomMapPoint point)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        if (!IsInterior(point.X, point.Y, width, height))
        {
            return false;
        }

        bool onValidSide;
        if (point.Y == room.Y - 1 || point.Y == room.Bottom)
        {
            onValidSide = point.X >= room.X && point.X < room.Right;
        }
        else if (point.X == room.X - 1 || point.X == room.Right)
        {
            onValidSide = point.Y >= room.Y && point.Y < room.Bottom;
        }
        else
        {
            return false;
        }

        if (!onValidSide)
        {
            return false;
        }

        RandomMapPoint outside = GetOutsideRoomPoint(room, point);
        return IsInterior(outside.X, outside.Y, width, height);
    }

    private static bool AreOppositeRoomOpenings(
        MapRect room,
        RandomMapPoint first,
        RandomMapPoint second)
    {
        RoomSide firstSide = GetRoomSide(room, first);
        RoomSide secondSide = GetRoomSide(room, second);
        return (firstSide == RoomSide.Top && secondSide == RoomSide.Bottom)
            || (firstSide == RoomSide.Bottom && secondSide == RoomSide.Top)
            || (firstSide == RoomSide.Left && secondSide == RoomSide.Right)
            || (firstSide == RoomSide.Right && secondSide == RoomSide.Left);
    }

    private static RoomSide GetRoomSide(MapRect room, RandomMapPoint point)
    {
        if (point.Y == room.Y - 1)
        {
            return RoomSide.Top;
        }

        if (point.Y == room.Bottom)
        {
            return RoomSide.Bottom;
        }

        if (point.X == room.X - 1)
        {
            return RoomSide.Left;
        }

        return RoomSide.Right;
    }

    private static double Score(
        GenerationCandidate candidate,
        RandomMapMode mode,
        int targetWalls)
    {
        bool[,] walls = candidate.Walls;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int playableHeight = height - 2;
        int playableWidth = width - 2;
        int wallCount = CountWalls(walls);
        double densityScore = targetWalls == 0
            ? 1
            : Math.Max(0, 1.0 - (targetWalls - wallCount) / (double)targetWalls);
        double balanceScore = CalculateBalanceScore(walls);

        if (mode == RandomMapMode.Lattice)
        {
            InteriorGraphMetrics metrics =
                CalculateInteriorGraphMetrics(walls, null);
            double cycleScore = Math.Min(
                1.0,
                metrics.CycleRank
                    / Math.Max(1.0, candidate.LatticeNodeCount / 10.0));
            double coreScore = metrics.Vertices == 0
                ? 0
                : Math.Min(1.0, metrics.CoreVertices / (metrics.Vertices * 0.45));
            double branchScore = Math.Min(
                1.0,
                metrics.Junctions / Math.Max(1.0, candidate.LatticeNodeCount / 8.0));
            double deadEndScore = metrics.Vertices == 0
                ? 0
                : Math.Max(0, 1.0 - metrics.DeadEnds / (metrics.Vertices * 0.35));
            double topologyScore =
                cycleScore * 0.35
                + coreScore * 0.30
                + branchScore * 0.20
                + deadEndScore * 0.15;
            return densityScore * 28.0
                + balanceScore * 12.0
                + topologyScore * 52.0;
        }

        HashSet<RandomMapPoint> roomInteriors = [];
        foreach (MapRect room in candidate.Rooms)
        {
            for (int y = room.Y; y < room.Bottom; y++)
            {
                for (int x = room.X; x < room.Right; x++)
                {
                    roomInteriors.Add(new RandomMapPoint(x, y));
                }
            }
        }

        InteriorGraphMetrics corridorMetrics =
            CalculateInteriorGraphMetrics(walls, roomInteriors);
        int longestRun = FindLongestOpenRun(walls);
        double runTarget =
            Math.Max(4.0, Math.Max(playableWidth, playableHeight) / 4.0);
        double runScore = Math.Min(1.0, longestRun / runTarget);
        double cycleTarget = Math.Max(1.0, candidate.Rooms.Count / 3.0);
        double roomCycleScore = Math.Min(
            1.0,
            corridorMetrics.CycleRank / cycleTarget);
        double roomCoreScore = corridorMetrics.Vertices == 0
            ? 0
            : Math.Min(
                1.0,
                corridorMetrics.CoreVertices / (corridorMetrics.Vertices * 0.35));
        int quadrantCount = candidate.Rooms
            .Select(
                room => (room.Center.Y >= height / 2 ? 2 : 0)
                    + (room.Center.X >= width / 2 ? 1 : 0))
            .Distinct()
            .Count();
        double dispersionScore = candidate.Rooms.Count == 0
            ? 0
            : Math.Min(1.0, quadrantCount / Math.Min(4.0, candidate.Rooms.Count));
        double routeScore = roomCycleScore * 0.6 + roomCoreScore * 0.4;
        return densityScore * 23.0
            + balanceScore * 12.0
            + runScore * 15.0
            + routeScore * 32.0
            + dispersionScore * 10.0;
    }

    private static double CalculateBalanceScore(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        double[] densities = new double[4];
        int[] totals = new int[4];

        int playableHeight = height - 2;
        int playableWidth = width - 2;

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                int quadrant = (y - 1 >= playableHeight / 2 ? 2 : 0)
                    + (x - 1 >= playableWidth / 2 ? 1 : 0);
                totals[quadrant]++;
                if (walls[y, x])
                {
                    densities[quadrant]++;
                }
            }
        }

        double average = 0;
        for (int index = 0; index < densities.Length; index++)
        {
            densities[index] /= Math.Max(1, totals[index]);
            average += densities[index];
        }

        average /= densities.Length;
        double deviation = 0;
        foreach (double value in densities)
        {
            deviation += Math.Abs(value - average);
        }

        return Math.Max(0, 1.0 - deviation);
    }

    private static int FindLongestOpenRun(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int longest = 0;

        for (int y = 1; y < height - 1; y++)
        {
            int current = 0;
            for (int x = 1; x < width - 1; x++)
            {
                current = walls[y, x] ? 0 : current + 1;
                longest = Math.Max(longest, current);
            }
        }

        for (int x = 1; x < width - 1; x++)
        {
            int current = 0;
            for (int y = 1; y < height - 1; y++)
            {
                current = walls[y, x] ? 0 : current + 1;
                longest = Math.Max(longest, current);
            }
        }

        return longest;
    }

    private static int CountWalls(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int count = 0;

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (walls[y, x])
                {
                    count++;
                }
            }
        }

        return count;
    }

    private static int CountOpenNeighbors(bool[,] walls, int x, int y)
    {
        int count = 0;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        foreach (RandomMapPoint direction in Directions)
        {
            int nextX = x + direction.X;
            int nextY = y + direction.Y;
            if (IsInterior(nextX, nextY, width, height)
                && !walls[nextY, nextX])
            {
                count++;
            }
        }

        return count;
    }

    private static int CountWallNeighbors(bool[,] walls, int x, int y)
    {
        int count = 0;
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        foreach (RandomMapPoint direction in Directions)
        {
            int nextX = x + direction.X;
            int nextY = y + direction.Y;
            if (nextX >= 0
                && nextY >= 0
                && nextX < width
                && nextY < height
                && walls[nextY, nextX])
            {
                count++;
            }
        }

        return count;
    }

    private static bool[,] CreateFilledWalls(int width, int height)
    {
        bool[,] walls = new bool[height, width];
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                walls[y, x] = true;
            }
        }

        return walls;
    }

    private static HashSet<RandomMapPoint> CollectVirtualBoundaryWalls(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        HashSet<RandomMapPoint> result = [];

        for (int x = 0; x < width; x++)
        {
            if (walls[0, x])
            {
                result.Add(new RandomMapPoint(x, 0));
            }

            if (walls[height - 1, x])
            {
                result.Add(new RandomMapPoint(x, height - 1));
            }
        }

        for (int y = 1; y < height - 1; y++)
        {
            if (walls[y, 0])
            {
                result.Add(new RandomMapPoint(0, y));
            }

            if (walls[y, width - 1])
            {
                result.Add(new RandomMapPoint(width - 1, y));
            }
        }

        return result;
    }

    private static void SealVirtualBoundary(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        for (int x = 0; x < width; x++)
        {
            walls[0, x] = true;
            walls[height - 1, x] = true;
        }

        for (int y = 1; y < height - 1; y++)
        {
            walls[y, 0] = true;
            walls[y, width - 1] = true;
        }
    }

    private static void OpenRectangle(bool[,] walls, MapRect rectangle)
    {
        for (int y = rectangle.Y; y < rectangle.Bottom; y++)
        {
            for (int x = rectangle.X; x < rectangle.Right; x++)
            {
                walls[y, x] = false;
            }
        }
    }

    private static void OpenPath(bool[,] walls, IEnumerable<RandomMapPoint> path)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        foreach (RandomMapPoint point in path)
        {
            if (IsInterior(point.X, point.Y, width, height))
            {
                walls[point.Y, point.X] = false;
            }
        }
    }

    private static RandomMapPoint FindNearest(
        RandomMapPoint source,
        IReadOnlyList<RandomMapPoint> candidates)
    {
        RandomMapPoint nearest = candidates[0];
        int nearestDistance = Manhattan(source, nearest);

        for (int index = 1; index < candidates.Count; index++)
        {
            int distance = Manhattan(source, candidates[index]);
            if (distance < nearestDistance)
            {
                nearest = candidates[index];
                nearestDistance = distance;
            }
        }

        return nearest;
    }

    private static int Manhattan(RandomMapPoint first, RandomMapPoint second)
    {
        return Math.Abs(first.X - second.X) + Math.Abs(first.Y - second.Y);
    }

    private static bool IsInterior(int x, int y, int width, int height)
    {
        return x > 0 && y > 0 && x < width - 1 && y < height - 1;
    }

    private static int GetPlayableArea(bool[,] walls)
    {
        return (walls.GetLength(1) - 2) * (walls.GetLength(0) - 2);
    }

    private static RandomMapPoint? FindFirstPlayableEmpty(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);

        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (!walls[y, x])
                {
                    return new RandomMapPoint(x, y);
                }
            }
        }

        return null;
    }

    private static IReadOnlyList<IReadOnlyList<int?>> CreateTiles(bool[,] walls, int wallTile)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        IReadOnlyList<int?>[] rows = new IReadOnlyList<int?>[height - 2];

        for (int y = 1; y < height - 1; y++)
        {
            int?[] row = new int?[width - 2];
            for (int x = 1; x < width - 1; x++)
            {
                row[x - 1] = walls[y, x] ? wallTile : null;
            }

            rows[y - 1] = Array.AsReadOnly(row);
        }

        return Array.AsReadOnly(rows);
    }

    private static RandomMapGenerationResult Failure(string error, int seed)
    {
        return new RandomMapGenerationResult(
            false,
            error,
            Array.Empty<IReadOnlyList<int?>>(),
            seed);
    }

    private static uint DeriveSeed(int seed, int candidateIndex)
    {
        uint value = unchecked((uint)seed) + 0x9E3779B9u * unchecked((uint)(candidateIndex + 1));
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        value *= 0x846CA68Bu;
        value ^= value >> 16;
        return value;
    }

    private readonly record struct LatticeEdge(
        RandomMapPoint From,
        RandomMapPoint To,
        int Direction);

    private readonly record struct InteriorGraphMetrics(
        int Vertices,
        int Edges,
        int Components,
        int LargestComponent,
        int CycleRank,
        int CoreVertices,
        int Junctions,
        int DeadEnds,
        int DenseVertices);

    private sealed record GenerationCandidate(
        bool[,] Walls,
        int LatticeNodeCount,
        IReadOnlyList<MapRect> Rooms,
        HashSet<RandomMapPoint> MainPath,
        Dictionary<MapRect, IReadOnlyList<RandomMapPoint>> RoomDoors,
        HashSet<RandomMapPoint> RoomNotches);

    private readonly record struct MapRect(int X, int Y, int Width, int Height)
    {
        public int Right => X + Width;
        public int Bottom => Y + Height;
        public RandomMapPoint Center => new RandomMapPoint(X + Width / 2, Y + Height / 2);

        public bool Contains(RandomMapPoint point)
        {
            return point.X >= X
                && point.Y >= Y
                && point.X < Right
                && point.Y < Bottom;
        }
    }

    private readonly record struct PathState(int X, int Y, int Direction);

    private enum RoomSide
    {
        Top,
        Bottom,
        Left,
        Right,
    }

    private readonly record struct PathPredecessor(
        int X,
        int Y,
        int Direction,
        bool HasValue);

    private sealed class StableRandom
    {
        private uint state;

        public StableRandom(uint seed)
        {
            state = seed == 0 ? 0xA341316Cu : seed;
        }

        public int Next(int exclusiveMaximum)
        {
            if (exclusiveMaximum <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(exclusiveMaximum));
            }

            return (int)(NextUInt32() % (uint)exclusiveMaximum);
        }

        public int NextInclusive(int inclusiveMinimum, int inclusiveMaximum)
        {
            if (inclusiveMaximum < inclusiveMinimum)
            {
                throw new ArgumentOutOfRangeException(nameof(inclusiveMaximum));
            }

            return inclusiveMinimum + Next(inclusiveMaximum - inclusiveMinimum + 1);
        }

        public void Shuffle<T>(IList<T> values)
        {
            for (int index = values.Count - 1; index > 0; index--)
            {
                int other = Next(index + 1);
                (values[index], values[other]) = (values[other], values[index]);
            }
        }

        private uint NextUInt32()
        {
            uint value = state;
            value ^= value << 13;
            value ^= value >> 17;
            value ^= value << 5;
            state = value;
            return value;
        }
    }
}
