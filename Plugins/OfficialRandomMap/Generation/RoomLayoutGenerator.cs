using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

internal static class RoomLayoutGenerator
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
        int area = width * height;
        int targetRooms = Math.Max(2, (int)Math.Round(area / 24.0));
        int minimumRooms = Math.Max(2, (int)Math.Ceiling(targetRooms * 0.7));
        int maximumRooms = Math.Max(minimumRooms, (int)Math.Ceiling(targetRooms * 1.4));
        if (width == 11 && height == 11)
        {
            minimumRooms = 4;
            maximumRooms = 7;
        }

        int minimumCycles = Math.Max(1, (int)Math.Round(area / 121.0));
        int maximumCycles = Math.Max(minimumCycles, (int)Math.Ceiling(area * 2.0 / 121));
        double density = targetWalls * 100.0 / area;
        if (area >= 144 && density < 34)
        {
            maximumRooms = Math.Max(maximumRooms, (area + 15) / 16);
        }

        int cycleTarget = minimumCycles + (int)Math.Round(
            (maximumCycles - minimumCycles) * Math.Clamp((45 - density) / 15, 0, 1));
        List<GridPlan> plans = CreatePlans(
            width, height, targetWalls, tolerance, minimumRooms, maximumRooms, cycleTarget,
            cancellationToken);
        random.Shuffle(plans);
        plans = plans.OrderBy(plan => Math.Abs(width * height
            - (width - plan.HorizontalMargin - plan.Columns + 1)
                * (height - plan.VerticalMargin - plan.Rows + 1)
            - plan.Columns * plan.Rows + 1 - cycleTarget - targetWalls)).ToList();
        foreach (GridPlan plan in plans.Take(24))
        {
            cancellationToken.ThrowIfCancellationRequested();
            int[] columnWidths = CreateLengths(
                width - plan.HorizontalMargin - plan.Columns + 1,
                plan.Columns, plan.WideColumns, random);
            int[] rowHeights = CreateLengths(
                height - plan.VerticalMargin - plan.Rows + 1,
                plan.Rows, plan.WideRows, random);
            int left = random.Next(plan.HorizontalMargin + 1);
            int top = random.Next(plan.VerticalMargin + 1);
            List<Chamber> chambers = (area >= 300 && density < 34) || random.Next(3) == 0
                ? CreateStaggeredChambers(width - plan.HorizontalMargin, rowHeights, left, top, random)
                : CreateChambers(columnWidths, rowHeights, left, top);
            int roomCount = chambers.Count(chamber => chamber.IsRoom);
            if (roomCount < minimumRooms || roomCount > maximumRooms)
            {
                continue;
            }

            List<Connection> edges = CreateConnections(chambers, random, cancellationToken);
            bool[,] walls = CreateWalls(width, height, chambers);
            List<RandomMapPoint> boundaryOpenings = [];
            HashSet<int> forcedEdges = [];
            if (!PlaceMarkers(walls, chambers, edges, markers, forcedEdges, boundaryOpenings))
            {
                continue;
            }

            HashSet<int>? leaves = SelectLeafRooms(
                chambers, edges, forcedEdges, cycleTarget, random, cancellationToken);
            if (leaves is null)
            {
                continue;
            }

            HashSet<int>? selected = ConnectChambers(
                chambers.Count, edges, forcedEdges, leaves, cycleTarget, maximumCycles, random,
                cancellationToken);
            if (selected is null)
            {
                continue;
            }

            if (!ChooseDoors(width, height, chambers, edges, selected, forcedEdges, leaves, random))
            {
                continue;
            }

            foreach (int edgeIndex in selected)
            {
                RandomMapPoint door = edges[edgeIndex].Door;
                walls[door.Y, door.X] = false;
            }

            foreach (RandomMapPoint opening in boundaryOpenings)
            {
                walls[opening.Y, opening.X] = false;
            }

            HashSet<RandomMapPoint> protectedWalls = CollectLeafWalls(
                width, height, chambers, leaves, edges, selected);
            if (!AdjustDensity(walls, protectedWalls, targetWalls, tolerance, cycleTarget, maximumCycles,
                    random, cancellationToken)
                || !Validate(walls, chambers, leaves, edges, selected, markers,
                    targetWalls, tolerance, cycleTarget, maximumCycles,
                    minimumRooms, maximumRooms, cancellationToken))
            {
                continue;
            }

            int gates = 0;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    gates += MapTopology.IsStraightGate(walls, x, y) ? 1 : 0;
                }
            }

            double quality = 30.0 * leaves.Count / roomCount
                + 20.0 * Math.Min(roomCount, targetRooms) / targetRooms
                + gates * 20.0 / area
                - Math.Abs(MapTopology.CountWalls(walls) - targetWalls) * 10.0 / area;
            return new MapLayout(walls, quality);
        }

        return null;
    }

    private static List<GridPlan> CreatePlans(
        int width, int height, int targetWalls, int tolerance,
        int minimumRooms, int maximumRooms, int cycles, CancellationToken cancellationToken)
    {
        List<GridPlan> plans = [];
        int maximumAdjustment = Math.Max(4, width * height / 12);
        for (int horizontalMargin = 0; horizontalMargin <= 2; horizontalMargin++)
        {
            for (int verticalMargin = 0; verticalMargin <= 2; verticalMargin++)
            {
                if (horizontalMargin == 2 && verticalMargin == 2)
                {
                    continue;
                }

                int innerWidth = width - horizontalMargin;
                int innerHeight = height - verticalMargin;
                for (int columns = Math.Max(2, (innerWidth + 3) / 4);
                    columns <= (innerWidth + 1) / 2; columns++)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    int totalWidth = innerWidth - columns + 1;
                    for (int rows = Math.Max(2, (innerHeight + 3) / 4);
                        rows <= (innerHeight + 1) / 2; rows++)
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                        int totalHeight = innerHeight - rows + 1;
                        int wallCount = width * height - totalWidth * totalHeight
                            - columns * rows + 1 - cycles;
                        if (wallCount < targetWalls - tolerance
                            || wallCount > targetWalls + tolerance + maximumAdjustment)
                        {
                            continue;
                        }

                        for (int wideColumns = (totalWidth - columns + 1) / 2;
                            wideColumns <= Math.Min(columns, totalWidth - columns); wideColumns++)
                        {
                            for (int wideRows = (totalHeight - rows + 1) / 2;
                                wideRows <= Math.Min(rows, totalHeight - rows); wideRows++)
                            {
                                int rooms = wideColumns * wideRows;
                                if (rooms >= minimumRooms && rooms <= maximumRooms)
                                {
                                    plans.Add(new GridPlan(columns, rows, wideColumns, wideRows,
                                        horizontalMargin, verticalMargin));
                                }
                            }
                        }
                    }
                }
            }
        }

        return plans;
    }

    private static int[] CreateLengths(int total, int count, int wide, StableRandom random)
    {
        List<int> lengths = Enumerable.Repeat(1, count).ToList();
        for (int index = 0; index < wide; index++)
        {
            lengths[index] = 2;
        }

        int remainder = total - count - wide;
        for (int index = 0; index < remainder; index++)
        {
            lengths[index]++;
        }

        random.Shuffle(lengths);
        return lengths.ToArray();
    }

    private static List<Chamber> CreateChambers(int[] widths, int[] heights, int left, int top)
    {
        List<Chamber> chambers = [];
        int y = top;
        foreach (int height in heights)
        {
            int x = left;
            foreach (int width in widths)
            {
                chambers.Add(new Chamber(x, y, width, height));
                x += width + 1;
            }

            y += height + 1;
        }

        return chambers;
    }

    private static List<Connection> CreateConnections(
        List<Chamber> chambers, StableRandom random, CancellationToken cancellationToken)
    {
        List<Connection> connections = [];
        for (int first = 0; first < chambers.Count; first++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            Chamber chamber = chambers[first];
            for (int second = first + 1; second < chambers.Count; second++)
            {
                Chamber next = chambers[second];
                int minY = Math.Max(chamber.Y, next.Y);
                int maxY = Math.Min(chamber.Bottom, next.Bottom);
                int minX = Math.Max(chamber.X, next.X);
                int maxX = Math.Min(chamber.Right, next.Right);
                if (chamber.Right + 1 == next.X && minY < maxY)
                {
                    connections.Add(new Connection(first, second,
                        new RandomMapPoint(chamber.Right, minY + random.Next(maxY - minY)), true));
                }

                if (chamber.Bottom + 1 == next.Y && minX < maxX)
                {
                    connections.Add(new Connection(first, second,
                        new RandomMapPoint(minX + random.Next(maxX - minX), chamber.Bottom), false));
                }
            }
        }

        return connections;
    }

    private static List<Chamber> CreateStaggeredChambers(
        int width, int[] heights, int left, int top, StableRandom random)
    {
        List<Chamber> chambers = [];
        int y = top;
        foreach (int height in heights)
        {
            int maximumWidth = height <= 2 ? 4 : 3;
            int count = (width + maximumWidth + 1) / (maximumWidth + 1);
            int total = width - count + 1;
            List<int> widths = Enumerable.Repeat(1, count).ToList();
            int remainder = total - count;
            for (int index = 0; remainder > 0; index++)
            {
                int addition = Math.Min(maximumWidth - 1, remainder);
                widths[index] += addition;
                remainder -= addition;
            }

            random.Shuffle(widths);
            int x = left;
            foreach (int chamberWidth in widths)
            {
                chambers.Add(new Chamber(x, y, chamberWidth, height));
                x += chamberWidth + 1;
            }

            y += height + 1;
        }

        return chambers;
    }

    private static bool ChooseDoors(
        int width, int height, List<Chamber> chambers, List<Connection> edges, HashSet<int> selected,
        HashSet<int> forced, HashSet<int> leaves, StableRandom random)
    {
        List<int> ordered = selected.OrderBy(index => forced.Contains(index) ? 0
            : leaves.Contains(edges[index].First) || leaves.Contains(edges[index].Second) ? 1 : 2).ToList();
        HashSet<RandomMapPoint> chosen = [];
        foreach (int index in ordered)
        {
            Connection edge = edges[index];
            Chamber first = chambers[edge.First];
            Chamber second = chambers[edge.Second];
            List<RandomMapPoint> options = [];
            if (forced.Contains(index))
            {
                options.Add(edge.Door);
            }
            else if (edge.Horizontal)
            {
                for (int y = Math.Max(first.Y, second.Y); y < Math.Min(first.Bottom, second.Bottom); y++)
                {
                    options.Add(new RandomMapPoint(first.Right, y));
                }
            }
            else
            {
                for (int x = Math.Max(first.X, second.X); x < Math.Min(first.Right, second.Right); x++)
                {
                    options.Add(new RandomMapPoint(x, first.Bottom));
                }
            }

            random.Shuffle(options);
            bool placed = false;
            foreach (RandomMapPoint point in options)
            {
                if ((leaves.Contains(edge.First) || leaves.Contains(edge.Second))
                    && (edge.Horizontal ? point.Y == 0 || point.Y == height - 1
                        : point.X == 0 || point.X == width - 1))
                {
                    continue;
                }

                if (chosen.Any(other => Math.Abs(other.X - point.X) + Math.Abs(other.Y - point.Y) <= 1))
                {
                    continue;
                }

                edges[index] = edge with { Door = point };
                chosen.Add(point);
                placed = true;
                break;
            }

            if (!placed)
            {
                return false;
            }
        }

        return true;
    }

    private static bool[,] CreateWalls(int width, int height, List<Chamber> chambers)
    {
        bool[,] walls = new bool[height, width];
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                walls[y, x] = true;
            }
        }

        foreach (Chamber chamber in chambers)
        {
            for (int y = chamber.Y; y < chamber.Bottom; y++)
            {
                for (int x = chamber.X; x < chamber.Right; x++)
                {
                    walls[y, x] = false;
                }
            }
        }

        return walls;
    }

    private static bool PlaceMarkers(
        bool[,] walls, List<Chamber> chambers, List<Connection> edges,
        IReadOnlySet<RandomMapPoint> markers, HashSet<int> forcedEdges,
        List<RandomMapPoint> boundaryOpenings)
    {
        int width = walls.GetLength(1);
        int height = walls.GetLength(0);
        foreach (RandomMapPoint marker in markers.OrderBy(point => point.Y).ThenBy(point => point.X))
        {
            if (!walls[marker.Y, marker.X])
            {
                continue;
            }

            int found = -1;
            for (int index = 0; index < edges.Count; index++)
            {
                Connection edge = edges[index];
                Chamber chamber = chambers[edge.First];
                Chamber next = chambers[edge.Second];
                if (edge.Horizontal
                    ? marker.X == chamber.Right && marker.Y >= Math.Max(chamber.Y, next.Y)
                        && marker.Y < Math.Min(chamber.Bottom, next.Bottom)
                    : marker.Y == chamber.Bottom && marker.X >= Math.Max(chamber.X, next.X)
                        && marker.X < Math.Min(chamber.Right, next.Right))
                {
                    found = index;
                    break;
                }
            }

            if (found >= 0)
            {
                if (forcedEdges.Contains(found) && edges[found].Door != marker)
                {
                    return false;
                }

                edges[found] = edges[found] with { Door = marker };
                forcedEdges.Add(found);
            }
            else if ((marker.X == 0 || marker.X == width - 1 || marker.Y == 0 || marker.Y == height - 1)
                && Neighbors(marker, width, height).Any(point => !walls[point.Y, point.X]))
            {
                boundaryOpenings.Add(marker);
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    private static HashSet<int>? SelectLeafRooms(
        List<Chamber> chambers, List<Connection> edges, HashSet<int> forcedEdges,
        int cycles, StableRandom random, CancellationToken cancellationToken)
    {
        int required = (chambers.Count(chamber => chamber.IsRoom) + 1) / 2;
        int[] forcedDegrees = new int[chambers.Count];
        foreach (int edgeIndex in forcedEdges)
        {
            forcedDegrees[edges[edgeIndex].First]++;
            forcedDegrees[edges[edgeIndex].Second]++;
        }

        for (int attempt = 0; attempt < 12; attempt++)
        {
            HashSet<int> leaves = [];
            List<int> candidates = Enumerable.Range(0, chambers.Count)
                .Where(index => chambers[index].IsRoom && forcedDegrees[index] <= 1).ToList();
            random.Shuffle(candidates);
            candidates = candidates.OrderBy(candidate => edges.Count(edge => edge.Touches(candidate))).ToList();
            foreach (int candidate in candidates)
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (forcedEdges.Any(index => edges[index].Touches(candidate)
                    && leaves.Contains(edges[index].Other(candidate))))
                {
                    continue;
                }

                leaves.Add(candidate);
                int coreEdges = edges.Count(edge => !leaves.Contains(edge.First) && !leaves.Contains(edge.Second));
                if (coreEdges - (chambers.Count - leaves.Count) + 1 < cycles
                    || !IsCoreConnected(chambers.Count, edges, leaves, cancellationToken)
                    || leaves.Any(leaf => !edges.Any(edge => edge.Touches(leaf)
                        && !leaves.Contains(edge.Other(leaf)))))
                {
                    leaves.Remove(candidate);
                }

                if (leaves.Count >= required)
                {
                    return leaves;
                }
            }
        }

        return null;
    }

    private static bool IsCoreConnected(
        int count, List<Connection> edges, HashSet<int> leaves, CancellationToken cancellationToken)
    {
        int start = Enumerable.Range(0, count).First(index => !leaves.Contains(index));
        HashSet<int> visited = [start];
        Queue<int> queue = new Queue<int>();
        queue.Enqueue(start);
        while (queue.TryDequeue(out int current))
        {
            cancellationToken.ThrowIfCancellationRequested();
            foreach (Connection edge in edges)
            {
                if (!edge.Touches(current))
                {
                    continue;
                }

                int next = edge.Other(current);
                if (!leaves.Contains(next) && visited.Add(next))
                {
                    queue.Enqueue(next);
                }
            }
        }

        return visited.Count == count - leaves.Count;
    }

    private static HashSet<int>? ConnectChambers(
        int count, List<Connection> edges, HashSet<int> forcedEdges,
        HashSet<int> leaves, int cycles, int maximumCycles, StableRandom random,
        CancellationToken cancellationToken)
    {
        HashSet<int> selected = [];
        int[] groups = Enumerable.Range(0, count).ToArray();
        int usedCycles = 0;
        foreach (int index in forcedEdges.Order())
        {
            Connection edge = edges[index];
            if (leaves.Contains(edge.First) && leaves.Contains(edge.Second))
            {
                return null;
            }

            usedCycles += Join(groups, edge.First, edge.Second) ? 0 : 1;
            selected.Add(index);
        }

        if (usedCycles > maximumCycles)
        {
            return null;
        }

        List<int> coreEdges = Enumerable.Range(0, edges.Count)
            .Where(index => !leaves.Contains(edges[index].First) && !leaves.Contains(edges[index].Second))
            .ToList();
        random.Shuffle(coreEdges);
        foreach (int index in coreEdges)
        {
            cancellationToken.ThrowIfCancellationRequested();
            Connection edge = edges[index];
            if (Join(groups, edge.First, edge.Second))
            {
                selected.Add(index);
            }
        }

        foreach (int leaf in leaves.Order())
        {
            if (selected.Any(index => edges[index].Touches(leaf)))
            {
                continue;
            }

            List<int> options = Enumerable.Range(0, edges.Count)
                .Where(index => edges[index].Touches(leaf)
                    && !leaves.Contains(edges[index].Other(leaf))).ToList();
            int index = options[random.Next(options.Count)];
            selected.Add(index);
            Join(groups, edges[index].First, edges[index].Second);
        }

        foreach (int index in coreEdges)
        {
            if (usedCycles >= cycles)
            {
                break;
            }

            if (selected.Add(index))
            {
                usedCycles++;
            }
        }

        return usedCycles >= cycles ? selected : null;
    }

    private static bool Join(int[] groups, int first, int second)
    {
        int from = groups[first];
        int to = groups[second];
        if (from == to)
        {
            return false;
        }

        for (int index = 0; index < groups.Length; index++)
        {
            if (groups[index] == from)
            {
                groups[index] = to;
            }
        }

        return true;
    }

    private static HashSet<RandomMapPoint> CollectLeafWalls(
        int width, int height, List<Chamber> chambers, HashSet<int> leaves,
        List<Connection> edges, HashSet<int> selected)
    {
        HashSet<RandomMapPoint> protectedWalls = [];
        foreach (int edgeIndex in selected)
        {
            Connection edge = edges[edgeIndex];
            int dx = edge.Horizontal ? 0 : 1;
            int dy = edge.Horizontal ? 1 : 0;
            protectedWalls.Add(new RandomMapPoint(edge.Door.X + dx, edge.Door.Y + dy));
            protectedWalls.Add(new RandomMapPoint(edge.Door.X - dx, edge.Door.Y - dy));
        }
        foreach (int leaf in leaves)
        {
            Chamber chamber = chambers[leaf];
            for (int y = Math.Max(0, chamber.Y - 1); y <= Math.Min(height - 1, chamber.Bottom); y++)
            {
                for (int x = Math.Max(0, chamber.X - 1); x <= Math.Min(width - 1, chamber.Right); x++)
                {
                    protectedWalls.Add(new RandomMapPoint(x, y));
                }
            }

            foreach (int edgeIndex in selected.Where(index => edges[index].Touches(leaf)))
            {
                Connection edge = edges[edgeIndex];
                int dx = edge.Horizontal ? 1 : 0;
                int dy = edge.Horizontal ? 0 : 1;
                protectedWalls.Add(new RandomMapPoint(edge.Door.X + dx, edge.Door.Y + dy));
                protectedWalls.Add(new RandomMapPoint(edge.Door.X - dx, edge.Door.Y - dy));
            }
        }

        return protectedWalls;
    }

    private static bool AdjustDensity(
        bool[,] walls, HashSet<RandomMapPoint> protectedWalls,
        int target, int tolerance, int cycles, int maximumCycles,
        StableRandom random, CancellationToken cancellationToken)
    {
        int width = walls.GetLength(1);
        int height = walls.GetLength(0);
        int count = MapTopology.CountWalls(walls);
        if (count < target - tolerance)
        {
            return false;
        }

        List<RandomMapPoint> options = [];
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                RandomMapPoint point = new RandomMapPoint(x, y);
                if (walls[y, x] && !protectedWalls.Contains(point))
                {
                    options.Add(point);
                }
            }
        }

        random.Shuffle(options);
        foreach (RandomMapPoint point in options)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (count <= target + tolerance)
            {
                break;
            }

            if (!Neighbors(point, width, height).Any(next => !walls[next.Y, next.X]))
            {
                continue;
            }

            walls[point.Y, point.X] = false;
            int actualCycles = MapTopology.CountRealCycles(walls, cancellationToken);
            if (actualCycles < cycles || actualCycles > maximumCycles || HasLargeSpace(walls, cancellationToken))
            {
                walls[point.Y, point.X] = true;
            }
            else
            {
                count--;
            }
        }

        return Math.Abs(count - target) <= tolerance;
    }

    private static bool Validate(
        bool[,] walls, List<Chamber> chambers, HashSet<int> leaves,
        List<Connection> edges, HashSet<int> selected, IReadOnlySet<RandomMapPoint> markers,
        int target, int tolerance, int cycles, int maximumCycles,
        int minimumRooms, int maximumRooms, CancellationToken cancellationToken)
    {
        int width = walls.GetLength(1);
        int height = walls.GetLength(0);
        int actualCycles = MapTopology.CountRealCycles(walls, cancellationToken);
        if (Math.Abs(MapTopology.CountWalls(walls) - target) > tolerance
            || markers.Any(marker => walls[marker.Y, marker.X])
            || !MapTopology.IsConnected(walls, cancellationToken) || MapTopology.HasWallBlock(walls)
            || HasLargeSpace(walls, cancellationToken)
            || selected.Any(index => !MapTopology.IsStraightGate(
                walls, edges[index].Door.X, edges[index].Door.Y, false))
            || actualCycles < cycles || actualCycles > maximumCycles)
        {
            return false;
        }

        List<HashSet<RandomMapPoint>> spaces = CollectSpaces(walls, cancellationToken);
        if (spaces.Count < minimumRooms || spaces.Count > maximumRooms)
        {
            return false;
        }

        int enclosed = 0;
        foreach (HashSet<RandomMapPoint> space in spaces)
        {
            HashSet<RandomMapPoint> openings = [];
            foreach (RandomMapPoint point in space)
            {
                foreach (RandomMapPoint next in Neighbors(point, width, height))
                {
                    if (!walls[next.Y, next.X] && !space.Contains(next))
                    {
                        openings.Add(next);
                    }
                }
            }

            if (openings.Count == 1)
            {
                RandomMapPoint gate = openings.First();
                if (MapTopology.IsStraightGate(walls, gate.X, gate.Y))
                {
                    enclosed++;
                }
            }
        }

        if (enclosed * 2 < spaces.Count)
        {
            return false;
        }

        foreach (int leaf in leaves)
        {
            cancellationToken.ThrowIfCancellationRequested();
            int[] doorEdges = selected.Where(index => edges[index].Touches(leaf)).ToArray();
            if (doorEdges.Length != 1)
            {
                return false;
            }

            RandomMapPoint door = edges[doorEdges[0]].Door;
            if (!MapTopology.IsStraightGate(walls, door.X, door.Y))
            {
                return false;
            }

            Chamber room = chambers[leaf];
            walls[door.Y, door.X] = true;
            HashSet<RandomMapPoint> reachable = [new RandomMapPoint(room.X, room.Y)];
            Queue<RandomMapPoint> queue = new Queue<RandomMapPoint>();
            queue.Enqueue(new RandomMapPoint(room.X, room.Y));
            while (queue.TryDequeue(out RandomMapPoint current))
            {
                cancellationToken.ThrowIfCancellationRequested();
                foreach (RandomMapPoint next in Neighbors(current, width, height))
                {
                    if (!walls[next.Y, next.X] && reachable.Add(next))
                    {
                        queue.Enqueue(next);
                    }
                }
            }

            walls[door.Y, door.X] = false;
            if (reachable.Count > 9 || reachable.Any(point => !room.Contains(point)
                && (!markers.Contains(point)
                    || (point.X > 0 && point.X < width - 1 && point.Y > 0 && point.Y < height - 1))))
            {
                return false;
            }
        }

        return true;
    }

    private static bool HasLargeSpace(bool[,] walls, CancellationToken cancellationToken)
    {
        return CollectSpaces(walls, cancellationToken).Any(space => space.Count > 9);
    }

    private static List<HashSet<RandomMapPoint>> CollectSpaces(bool[,] walls, CancellationToken cancellationToken)
    {
        List<HashSet<RandomMapPoint>> spaces = [];
        int width = walls.GetLength(1);
        int height = walls.GetLength(0);
        bool[,] excluded = new bool[height, width];
        for (int y = 0; y < height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < width; x++)
            {
                excluded[y, x] = walls[y, x] || MapTopology.IsStraightGate(walls, x, y, false);
            }
        }

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (excluded[y, x])
                {
                    continue;
                }

                HashSet<RandomMapPoint> component = [new RandomMapPoint(x, y)];
                Queue<RandomMapPoint> pending = new Queue<RandomMapPoint>();
                pending.Enqueue(new RandomMapPoint(x, y));
                excluded[y, x] = true;
                while (pending.TryDequeue(out RandomMapPoint point))
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    foreach (RandomMapPoint neighbor in Neighbors(point, width, height))
                    {
                        if (!excluded[neighbor.Y, neighbor.X])
                        {
                            excluded[neighbor.Y, neighbor.X] = true;
                            component.Add(neighbor);
                            pending.Enqueue(neighbor);
                        }
                    }
                }

                if (component.Any(point =>
                    component.Contains(new RandomMapPoint(point.X + 1, point.Y))
                    && component.Contains(new RandomMapPoint(point.X, point.Y + 1))
                    && component.Contains(new RandomMapPoint(point.X + 1, point.Y + 1))))
                {
                    spaces.Add(component);
                }
            }
        }

        return spaces;
    }

    private static IEnumerable<RandomMapPoint> Neighbors(RandomMapPoint point, int width, int height)
    {
        if (point.X > 0)
        {
            yield return new RandomMapPoint(point.X - 1, point.Y);
        }

        if (point.X + 1 < width)
        {
            yield return new RandomMapPoint(point.X + 1, point.Y);
        }

        if (point.Y > 0)
        {
            yield return new RandomMapPoint(point.X, point.Y - 1);
        }

        if (point.Y + 1 < height)
        {
            yield return new RandomMapPoint(point.X, point.Y + 1);
        }
    }

    private readonly record struct GridPlan(
        int Columns, int Rows, int WideColumns, int WideRows, int HorizontalMargin, int VerticalMargin);

    private readonly record struct Chamber(int X, int Y, int Width, int Height)
    {
        internal int Right => X + Width;
        internal int Bottom => Y + Height;
        internal bool IsRoom => Width > 1 && Height > 1;

        internal bool Contains(RandomMapPoint point)
        {
            return point.X >= X && point.X < Right && point.Y >= Y && point.Y < Bottom;
        }
    }

    private readonly record struct Connection(int First, int Second, RandomMapPoint Door, bool Horizontal)
    {
        internal bool Touches(int chamber) => First == chamber || Second == chamber;
        internal int Other(int chamber) => First == chamber ? Second : First;
    }
}
