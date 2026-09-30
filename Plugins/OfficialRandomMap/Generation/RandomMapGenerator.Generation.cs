using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public static partial class RandomMapGenerator
{
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
}
