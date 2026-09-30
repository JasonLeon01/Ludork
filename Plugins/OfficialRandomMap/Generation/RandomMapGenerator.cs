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

public static partial class RandomMapGenerator
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

    private static bool HasSingleEmptyComponent(bool[,] walls)
    {
        LabelEmptyComponents(walls, out int componentCount);
        return componentCount == 1;
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
