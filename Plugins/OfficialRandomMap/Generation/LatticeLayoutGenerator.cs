using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

internal static class LatticeLayoutGenerator
{
    internal static MapLayout? Generate(
        int width,
        int height,
        IReadOnlySet<RandomMapPoint> markers,
        int targetWalls,
        int tolerance,
        StableRandom random,
        CancellationToken cancellationToken)
    {
        bool[,] walls = new bool[height, width];
        bool[,] protectedWalls = new bool[height, width];
        bool[,] protectedOpen = new bool[height, width];
        int gateBudget = Math.Max(2, (int)Math.Round(width * height / 32.0));
        List<RandomMapPoint> gates = PlaceGateways(
            walls, markers, protectedWalls, protectedOpen, gateBudget, random, cancellationToken);
        if (gates.Count < gateBudget)
        {
            return null;
        }

        Fill(walls, markers, protectedOpen, targetWalls, random, cancellationToken);
        int penalty = DensityPenalty(walls, targetWalls, tolerance, cancellationToken);
        int attempts = Math.Clamp(width * height / 3, 24, 160);
        for (int attempt = 0; penalty > 0 && attempt < attempts; attempt++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            List<List<RandomMapPoint>> components = CollectMovableWalls(walls, protectedWalls);
            if (components.Count == 0)
            {
                break;
            }

            bool[,] trial = (bool[,])walls.Clone();
            random.Shuffle(components);
            int removeCount = Math.Min(components.Count, random.NextInclusive(1, 3));
            foreach (List<RandomMapPoint> component in components.Take(removeCount))
            {
                foreach (RandomMapPoint point in component)
                {
                    trial[point.Y, point.X] = false;
                }
            }
            Fill(trial, markers, protectedOpen, targetWalls, random, cancellationToken);
            int trialPenalty = DensityPenalty(trial, targetWalls, tolerance, cancellationToken);
            if (trialPenalty < penalty || (trialPenalty == penalty && random.Next(2) == 0))
            {
                walls = trial;
                penalty = trialPenalty;
            }
        }

        if (penalty != 0 || gates.Any(point => !MapTopology.IsStraightGate(walls, point.X, point.Y)))
        {
            return null;
        }

        int wallCount = MapTopology.CountWalls(walls);
        int openCount = width * height - wallCount;
        int gateCount = MapTopology.CountStraightGates(walls);
        double gateTarget = Math.Max(gateBudget, openCount * 0.18);
        double quality = Math.Min(1.0, gateCount / gateTarget) * 0.2
            - CountRepeatingWalls(walls) * 2.0 / (width * height)
            - Math.Max(0, DominantParity(walls, wallCount) - 0.45) * 0.4;
        return new MapLayout(walls, quality);
    }

    private static List<RandomMapPoint> PlaceGateways(
        bool[,] walls,
        IReadOnlySet<RandomMapPoint> markers,
        bool[,] protectedWalls,
        bool[,] protectedOpen,
        int budget,
        StableRandom random,
        CancellationToken cancellationToken)
    {
        int width = walls.GetLength(1);
        int height = walls.GetLength(0);
        List<RandomMapPoint> positions = [];
        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                positions.Add(new RandomMapPoint(x, y));
            }
        }
        random.Shuffle(positions);
        List<RandomMapPoint> gates = [];
        foreach (RandomMapPoint center in positions)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (gates.Count == budget)
            {
                break;
            }
            if (walls[center.Y, center.X]
                || gates.Any(gate => Math.Abs(gate.X - center.X) + Math.Abs(gate.Y - center.Y) < 3))
            {
                continue;
            }
            int orientation = random.Next(2);
            for (int choice = 0; choice < 2; choice++)
            {
                RandomMapPoint direction = MapTopology.Directions[(orientation + choice) % 2];
                RandomMapPoint perpendicular = new RandomMapPoint(direction.Y, direction.X);
                RandomMapPoint first = new RandomMapPoint(center.X - direction.X, center.Y - direction.Y);
                RandomMapPoint second = new RandomMapPoint(center.X + direction.X, center.Y + direction.Y);
                if (walls[center.Y - perpendicular.Y, center.X - perpendicular.X]
                    || walls[center.Y + perpendicular.Y, center.X + perpendicular.X]
                    || !CanPlaceSegment(walls, first, direction, 1, markers, protectedOpen)
                    || !CanPlaceSegment(walls, second, direction, 1, markers, protectedOpen))
                {
                    continue;
                }

                walls[first.Y, first.X] = true;
                walls[second.Y, second.X] = true;
                if (!MapTopology.IsConnected(walls, cancellationToken))
                {
                    walls[first.Y, first.X] = false;
                    walls[second.Y, second.X] = false;
                    continue;
                }
                protectedWalls[first.Y, first.X] = true;
                protectedWalls[second.Y, second.X] = true;
                protectedOpen[center.Y, center.X] = true;
                protectedOpen[center.Y - perpendicular.Y, center.X - perpendicular.X] = true;
                protectedOpen[center.Y + perpendicular.Y, center.X + perpendicular.X] = true;
                gates.Add(center);
                break;
            }
        }
        return gates;
    }

    private static void Fill(
        bool[,] walls,
        IReadOnlySet<RandomMapPoint> markers,
        bool[,] protectedOpen,
        int target,
        StableRandom random,
        CancellationToken cancellationToken)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        int count = MapTopology.CountWalls(walls);
        List<RandomMapPoint> windows = [];
        for (int y = 0; y <= height - 4; y++)
        {
            for (int x = 0; x <= width - 4; x++)
            {
                windows.Add(new RandomMapPoint(x, y));
            }
        }
        random.Shuffle(windows);
        foreach (RandomMapPoint window in windows)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (count >= target)
            {
                break;
            }
            bool empty = true;
            for (int y = window.Y; y < window.Y + 4 && empty; y++)
            {
                for (int x = window.X; x < window.X + 4; x++)
                {
                    if (walls[y, x])
                    {
                        empty = false;
                        break;
                    }
                }
            }
            if (!empty)
            {
                continue;
            }
            List<RandomMapPoint> options = [];
            for (int y = window.Y; y < window.Y + 4; y++)
            {
                for (int x = window.X; x < window.X + 4; x++)
                {
                    options.Add(new RandomMapPoint(x, y));
                }
            }
            random.Shuffle(options);
            foreach (RandomMapPoint point in options)
            {
                int added = TryPlaceWall(walls, point, markers, protectedOpen, target - count, random, cancellationToken);
                if (added > 0)
                {
                    count += added;
                    break;
                }
            }
        }

        List<RandomMapPoint> positions = [];
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (!walls[y, x] && !protectedOpen[y, x])
                {
                    positions.Add(new RandomMapPoint(x, y));
                }
            }
        }
        random.Shuffle(positions);
        foreach (RandomMapPoint point in positions)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (count >= target)
            {
                break;
            }
            count += TryPlaceWall(walls, point, markers, protectedOpen, target - count, random, cancellationToken);
        }
    }

    private static int TryPlaceWall(
        bool[,] walls,
        RandomMapPoint point,
        IReadOnlySet<RandomMapPoint> markers,
        bool[,] protectedOpen,
        int remaining,
        StableRandom random,
        CancellationToken cancellationToken)
    {
        int selection = random.Next(10);
        int length = Math.Min(remaining, selection < 5 ? 1 : selection < 8 ? 2 : 3);
        RandomMapPoint direction = MapTopology.Directions[random.Next(4)];
        for (; length >= 1; length--)
        {
            if (!CanPlaceSegment(walls, point, direction, length, markers, protectedOpen))
            {
                continue;
            }
            for (int index = 0; index < length; index++)
            {
                walls[point.Y + index * direction.Y, point.X + index * direction.X] = true;
            }
            if (MapTopology.IsConnected(walls, cancellationToken))
            {
                return length;
            }
            for (int index = 0; index < length; index++)
            {
                walls[point.Y + index * direction.Y, point.X + index * direction.X] = false;
            }
        }
        return 0;
    }

    private static bool CanPlaceSegment(
        bool[,] walls,
        RandomMapPoint point,
        RandomMapPoint direction,
        int length,
        IReadOnlySet<RandomMapPoint> markers,
        bool[,] protectedOpen)
    {
        for (int index = 0; index < length; index++)
        {
            int x = point.X + index * direction.X;
            int y = point.Y + index * direction.Y;
            if (!MapTopology.Contains(walls, x, y) || walls[y, x]
                || protectedOpen[y, x] || markers.Contains(new RandomMapPoint(x, y)))
            {
                return false;
            }
            foreach (RandomMapPoint neighbor in MapTopology.Directions)
            {
                int nextX = x + neighbor.X;
                int nextY = y + neighbor.Y;
                if (MapTopology.Contains(walls, nextX, nextY) && walls[nextY, nextX])
                {
                    return false;
                }
            }
        }
        return true;
    }

    private static List<List<RandomMapPoint>> CollectMovableWalls(bool[,] walls, bool[,] protectedWalls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        bool[,] visited = new bool[height, width];
        List<List<RandomMapPoint>> components = [];
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (!walls[y, x] || visited[y, x])
                {
                    continue;
                }
                List<RandomMapPoint> component = [];
                Queue<RandomMapPoint> pending = new Queue<RandomMapPoint>();
                pending.Enqueue(new RandomMapPoint(x, y));
                visited[y, x] = true;
                bool movable = true;
                while (pending.TryDequeue(out RandomMapPoint point))
                {
                    component.Add(point);
                    movable &= !protectedWalls[point.Y, point.X];
                    foreach (RandomMapPoint direction in MapTopology.Directions)
                    {
                        int nextX = point.X + direction.X;
                        int nextY = point.Y + direction.Y;
                        if (MapTopology.Contains(walls, nextX, nextY)
                            && walls[nextY, nextX] && !visited[nextY, nextX])
                        {
                            visited[nextY, nextX] = true;
                            pending.Enqueue(new RandomMapPoint(nextX, nextY));
                        }
                    }
                }
                if (movable)
                {
                    components.Add(component);
                }
            }
        }
        return components;
    }

    private static int DensityPenalty(bool[,] walls, int target, int tolerance, CancellationToken cancellationToken)
    {
        int error = Math.Max(0, Math.Abs(MapTopology.CountWalls(walls) - target) - tolerance);
        return error * 4 + (MapTopology.HasEmptyWindow(walls, 4, cancellationToken) ? 1 : 0);
    }

    private static int CountRepeatingWalls(bool[,] walls)
    {
        int count = 0;
        for (int y = 0; y < walls.GetLength(0); y++)
        {
            for (int x = 0; x < walls.GetLength(1); x++)
            {
                foreach (RandomMapPoint direction in MapTopology.Directions.Take(2))
                {
                    if (!MapTopology.Contains(walls, x + direction.X * 6, y + direction.Y * 6))
                    {
                        continue;
                    }
                    bool repeating = true;
                    for (int index = 0; index < 7; index++)
                    {
                        if (walls[y + direction.Y * index, x + direction.X * index] != (index % 2 == 0))
                        {
                            repeating = false;
                            break;
                        }
                    }
                    if (repeating)
                    {
                        count++;
                    }
                }
            }
        }
        return count;
    }

    private static double DominantParity(bool[,] walls, int wallCount)
    {
        int[] counts = new int[4];
        for (int y = 0; y < walls.GetLength(0); y++)
        {
            for (int x = 0; x < walls.GetLength(1); x++)
            {
                if (walls[y, x])
                {
                    counts[(y % 2) * 2 + x % 2]++;
                }
            }
        }
        return counts.Max() / (double)Math.Max(1, wallCount);
    }
}
