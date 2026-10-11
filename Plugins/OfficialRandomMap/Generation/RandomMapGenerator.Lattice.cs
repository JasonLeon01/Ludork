using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public static partial class RandomMapGenerator
{
    private static GenerationCandidate GenerateLattice(
        int width,
        int height,
        HashSet<RandomMapPoint> markers,
        int targetWalls,
        StableRandom random,
        CancellationToken cancellationToken)
    {
        bool[,] walls = new bool[height, width];
        SealVirtualBoundary(walls);
        int wallCount = 0;

        while (TryFindEmptyWindow(walls, 4, out MapRect window))
        {
            cancellationToken.ThrowIfCancellationRequested();
            List<RandomMapPoint> positions = [];
            for (int y = window.Y; y < window.Bottom; y++)
            {
                for (int x = window.X; x < window.Right; x++)
                {
                    positions.Add(new RandomMapPoint(x, y));
                }
            }

            random.Shuffle(positions);
            bool placed = false;
            foreach (RandomMapPoint position in positions)
            {
                int length = TryPlaceLatticeWall(
                    walls, position, markers, targetWalls - wallCount, random);
                if (length > 0)
                {
                    wallCount += length;
                    placed = true;
                    break;
                }
            }

            if (!placed)
            {
                break;
            }
        }

        List<RandomMapPoint> candidates = [];
        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                candidates.Add(new RandomMapPoint(x, y));
            }
        }

        random.Shuffle(candidates);
        foreach (RandomMapPoint position in candidates)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (wallCount >= targetWalls)
            {
                break;
            }

            wallCount += TryPlaceLatticeWall(
                walls, position, markers, targetWalls - wallCount, random);
        }

        return new GenerationCandidate(walls, [], [], [], []);
    }

    private static int TryPlaceLatticeWall(
        bool[,] walls,
        RandomMapPoint position,
        HashSet<RandomMapPoint> markers,
        int remainingWalls,
        StableRandom random)
    {
        if (remainingWalls <= 0)
        {
            return 0;
        }

        int selection = random.Next(6);
        int length = selection < 3 ? 1 : selection < 5 ? 2 : 3;
        RandomMapPoint direction = Directions[random.Next(Directions.Length)];
        if (length <= remainingWalls
            && TryPlaceLatticeSegment(walls, position, direction, length, markers))
        {
            return length;
        }

        return length > 1
            && TryPlaceLatticeSegment(walls, position, direction, 1, markers)
                ? 1
                : 0;
    }

    private static bool TryPlaceLatticeSegment(
        bool[,] walls,
        RandomMapPoint position,
        RandomMapPoint direction,
        int length,
        HashSet<RandomMapPoint> markers)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        for (int index = 0; index < length; index++)
        {
            int x = position.X + direction.X * index;
            int y = position.Y + direction.Y * index;
            if (!IsInterior(x, y, width, height)
                || walls[y, x]
                || markers.Contains(new RandomMapPoint(x, y)))
            {
                return false;
            }

            foreach (RandomMapPoint neighbor in Directions)
            {
                int neighborX = x + neighbor.X;
                int neighborY = y + neighbor.Y;
                if (IsInterior(neighborX, neighborY, width, height)
                    && walls[neighborY, neighborX])
                {
                    return false;
                }
            }
        }

        for (int index = 0; index < length; index++)
        {
            walls[position.Y + direction.Y * index, position.X + direction.X * index] = true;
        }

        bool valid = HasSingleEmptyComponent(walls) && !HasWallBlock(walls);
        if (!valid)
        {
            for (int index = 0; index < length; index++)
            {
                walls[position.Y + direction.Y * index, position.X + direction.X * index] = false;
            }
        }

        return valid;
    }

    private static bool ValidateLattice(
        bool[,] walls,
        HashSet<RandomMapPoint> markers,
        int targetWalls,
        out string failure)
    {
        failure = string.Empty;
        if (markers.Any(point => walls[point.Y, point.X])
            || !HasSealedVirtualBoundary(walls))
        {
            failure = "noValidTopology";
        }
        else if (CountWalls(walls) > targetWalls)
        {
            failure = "noSafeDensity";
        }
        else if (!HasSingleEmptyComponent(walls))
        {
            failure = "noConnectedLayout";
        }
        else if (HasWallBlock(walls))
        {
            failure = "noWallBlocks";
        }
        else if (HasEmptyWindow(walls, 4))
        {
            failure = "noUniformLayout";
        }
        else if (!HasValidLatticeComponents(walls))
        {
            failure = "latticeStyleFailed";
        }

        return failure.Length == 0;
    }

    private static bool HasValidLatticeComponents(bool[,] walls)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        bool[,] visited = new bool[height, width];
        Queue<RandomMapPoint> pending = new Queue<RandomMapPoint>();
        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                if (!walls[y, x] || visited[y, x])
                {
                    continue;
                }

                int length = 0;
                bool horizontal = true;
                bool vertical = true;
                visited[y, x] = true;
                pending.Enqueue(new RandomMapPoint(x, y));
                while (pending.TryDequeue(out RandomMapPoint point))
                {
                    length++;
                    horizontal &= point.Y == y;
                    vertical &= point.X == x;
                    if (length > 3 || (!horizontal && !vertical))
                    {
                        return false;
                    }

                    foreach (RandomMapPoint direction in Directions)
                    {
                        int nextX = point.X + direction.X;
                        int nextY = point.Y + direction.Y;
                        if (IsInterior(nextX, nextY, width, height)
                            && walls[nextY, nextX]
                            && !visited[nextY, nextX])
                        {
                            visited[nextY, nextX] = true;
                            pending.Enqueue(new RandomMapPoint(nextX, nextY));
                        }
                    }
                }
            }
        }

        return true;
    }
}
