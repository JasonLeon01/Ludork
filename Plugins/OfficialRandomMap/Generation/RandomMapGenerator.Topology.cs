using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public static partial class RandomMapGenerator
{
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
}
