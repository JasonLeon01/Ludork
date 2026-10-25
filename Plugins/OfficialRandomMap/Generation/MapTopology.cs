using System;
using System.Collections.Generic;
using System.Threading;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

internal static class MapTopology
{
    internal static readonly RandomMapPoint[] Directions =
    [
        new RandomMapPoint(1, 0),
        new RandomMapPoint(0, 1),
        new RandomMapPoint(-1, 0),
        new RandomMapPoint(0, -1),
    ];

    internal static bool Contains(bool[,] walls, int x, int y)
    {
        return x >= 0 && y >= 0 && x < walls.GetLength(1) && y < walls.GetLength(0);
    }

    internal static bool IsOpen(bool[,] walls, int x, int y)
    {
        return Contains(walls, x, y) && !walls[y, x];
    }

    internal static int CountWalls(bool[,] walls)
    {
        int count = 0;
        foreach (bool wall in walls)
        {
            if (wall)
            {
                count++;
            }
        }
        return count;
    }

    internal static bool HasWallBlock(bool[,] walls)
    {
        for (int y = 1; y < walls.GetLength(0); y++)
        {
            for (int x = 1; x < walls.GetLength(1); x++)
            {
                if (walls[y, x] && walls[y - 1, x] && walls[y, x - 1] && walls[y - 1, x - 1])
                {
                    return true;
                }
            }
        }
        return false;
    }

    internal static bool IsConnected(bool[,] walls, CancellationToken cancellationToken = default)
    {
        return CountComponents(walls, cancellationToken) == 1;
    }

    private static int CountComponents(bool[,] walls, CancellationToken cancellationToken)
    {
        int height = walls.GetLength(0);
        int width = walls.GetLength(1);
        bool[,] visited = new bool[height, width];
        Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();
        int components = 0;
        int processed = 0;
        for (int y = 0; y < height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < width; x++)
            {
                if (walls[y, x] || visited[y, x])
                {
                    continue;
                }
                components++;
                visited[y, x] = true;
                queue.Enqueue(new RandomMapPoint(x, y));
                while (queue.TryDequeue(out RandomMapPoint point))
                {
                    if ((processed++ & 255) == 0)
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                    }
                    foreach (RandomMapPoint direction in Directions)
                    {
                        int nextX = point.X + direction.X;
                        int nextY = point.Y + direction.Y;
                        if (IsOpen(walls, nextX, nextY) && !visited[nextY, nextX])
                        {
                            visited[nextY, nextX] = true;
                            queue.Enqueue(new RandomMapPoint(nextX, nextY));
                        }
                    }
                }
            }
        }
        return components;
    }

    internal static bool HasEmptyWindow(bool[,] walls, int size, CancellationToken cancellationToken = default)
    {
        for (int y = 0; y <= walls.GetLength(0) - size; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x <= walls.GetLength(1) - size; x++)
            {
                bool empty = true;
                for (int dy = 0; dy < size && empty; dy++)
                {
                    for (int dx = 0; dx < size; dx++)
                    {
                        if (walls[y + dy, x + dx])
                        {
                            empty = false;
                            break;
                        }
                    }
                }
                if (empty)
                {
                    return true;
                }
            }
        }
        return false;
    }

    internal static bool IsStraightGate(bool[,] walls, int x, int y, bool requireActualWalls = true)
    {
        if (!IsOpen(walls, x, y))
        {
            return false;
        }
        bool horizontal = IsOpen(walls, x - 1, y) && IsOpen(walls, x + 1, y)
            && !IsOpen(walls, x, y - 1) && !IsOpen(walls, x, y + 1)
            && (!requireActualWalls || (y > 0 && y < walls.GetLength(0) - 1));
        bool vertical = IsOpen(walls, x, y - 1) && IsOpen(walls, x, y + 1)
            && !IsOpen(walls, x - 1, y) && !IsOpen(walls, x + 1, y)
            && (!requireActualWalls || (x > 0 && x < walls.GetLength(1) - 1));
        return horizontal || vertical;
    }

    internal static int CountStraightGates(bool[,] walls)
    {
        int count = 0;
        for (int y = 0; y < walls.GetLength(0); y++)
        {
            for (int x = 0; x < walls.GetLength(1); x++)
            {
                if (IsStraightGate(walls, x, y))
                {
                    count++;
                }
            }
        }
        return count;
    }

    internal static int CountRealCycles(bool[,] walls, CancellationToken cancellationToken = default)
    {
        int vertices = 0;
        int edges = 0;
        int faces = 0;
        for (int y = 0; y < walls.GetLength(0); y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < walls.GetLength(1); x++)
            {
                if (walls[y, x])
                {
                    continue;
                }
                vertices++;
                bool right = IsOpen(walls, x + 1, y);
                bool down = IsOpen(walls, x, y + 1);
                edges += (right ? 1 : 0) + (down ? 1 : 0);
                if (right && down && IsOpen(walls, x + 1, y + 1))
                {
                    faces++;
                }
            }
        }
        return edges - vertices + CountComponents(walls, cancellationToken) - faces;
    }

}
