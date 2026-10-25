using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialMotaMapFill.Generation;

internal sealed class MotaMapTopology
{
    private static readonly (int X, int Y, MotaConnections Out, MotaConnections In)[] directions =
    [
        (0, -1, MotaConnections.Up, MotaConnections.Down),
        (1, 0, MotaConnections.Right, MotaConnections.Left),
        (0, 1, MotaConnections.Down, MotaConnections.Up),
        (-1, 0, MotaConnections.Left, MotaConnections.Right),
    ];

    private readonly MotaMapFillRequest request;
    private readonly CancellationToken cancellationToken;
    private readonly int[,] roomIds;
    private readonly List<Room> rooms = [];
    private readonly List<Corridor> corridors = [];
    private readonly List<Pocket> pockets = [];

    public MotaMapTopology(MotaMapFillRequest request, CancellationToken cancellationToken)
    {
        this.request = request;
        this.cancellationToken = cancellationToken;
        Width = request.Walkable.GetLength(1);
        Height = request.Walkable.GetLength(0);
        roomIds = new int[Height, Width];
        for (int y = 0; y < Height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < Width; x++)
            {
                roomIds[y, x] = -1;
            }
        }
        FindRooms();
        FindCorridors();
        FindPockets();
    }

    public int Width { get; }
    public int Height { get; }
    public IReadOnlyList<Room> Rooms => rooms;
    public IReadOnlyList<Corridor> Corridors => corridors;
    public IReadOnlyList<Pocket> Pockets => pockets;

    public bool IsWalkable(MotaCell cell)
    {
        return cell.X >= 0 && cell.Y >= 0 && cell.X < Width && cell.Y < Height && request.Walkable[cell.Y, cell.X];
    }

    public int RoomId(MotaCell cell)
    {
        return roomIds[cell.Y, cell.X];
    }

    public IEnumerable<MotaCell> Neighbors(MotaCell cell)
    {
        foreach ((int x, int y, MotaConnections outgoing, MotaConnections incoming) in directions)
        {
            MotaCell neighbor = new MotaCell(cell.X + x, cell.Y + y);
            if (IsWalkable(neighbor)
                && (request.Connections[cell.Y, cell.X] & outgoing) != 0
                && (request.Connections[neighbor.Y, neighbor.X] & incoming) != 0)
            {
                yield return neighbor;
            }
        }
    }

    public bool IsGate(MotaCell cell)
    {
        return RoomId(cell) < 0 && Neighbors(cell).Take(3).Count() == 2;
    }

    public bool IsStraightGate(MotaCell cell)
    {
        MotaCell[] neighbors = Neighbors(cell).ToArray();
        return neighbors.Length == 2
            && neighbors[0].X + neighbors[1].X == cell.X * 2
            && neighbors[0].Y + neighbors[1].Y == cell.Y * 2;
    }

    private bool HasConnection(MotaCell first, MotaCell second)
    {
        return IsWalkable(first) && Neighbors(first).Contains(second);
    }

    private void FindRooms()
    {
        bool[,] interior = new bool[Height, Width];
        for (int y = 0; y + 1 < Height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x + 1 < Width; x++)
            {
                MotaCell topLeft = new MotaCell(x, y);
                MotaCell topRight = new MotaCell(x + 1, y);
                MotaCell bottomLeft = new MotaCell(x, y + 1);
                MotaCell bottomRight = new MotaCell(x + 1, y + 1);
                if (HasConnection(topLeft, topRight) && HasConnection(topLeft, bottomLeft)
                    && HasConnection(topRight, bottomRight) && HasConnection(bottomLeft, bottomRight))
                {
                    interior[y, x] = true;
                    interior[y, x + 1] = true;
                    interior[y + 1, x] = true;
                    interior[y + 1, x + 1] = true;
                }
            }
        }

        Queue<MotaCell> pending = new Queue<MotaCell>();
        for (int y = 0; y < Height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < Width; x++)
            {
                if (!interior[y, x] || roomIds[y, x] >= 0)
                {
                    continue;
                }
                int roomId = rooms.Count;
                List<MotaCell> cells = [];
                roomIds[y, x] = roomId;
                pending.Enqueue(new MotaCell(x, y));
                while (pending.TryDequeue(out MotaCell cell))
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    cells.Add(cell);
                    foreach (MotaCell neighbor in Neighbors(cell))
                    {
                        if (interior[neighbor.Y, neighbor.X] && roomIds[neighbor.Y, neighbor.X] < 0)
                        {
                            roomIds[neighbor.Y, neighbor.X] = roomId;
                            pending.Enqueue(neighbor);
                        }
                    }
                }
                cells.Sort(CompareCells);
                rooms.Add(new Room(roomId, cells));
            }
        }
    }

    private void FindCorridors()
    {
        HashSet<MotaCell> nodes = [];
        List<MotaCell> passageCells = [];
        for (int y = 0; y < Height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < Width; x++)
            {
                MotaCell cell = new MotaCell(x, y);
                if (!IsWalkable(cell) || RoomId(cell) >= 0)
                {
                    continue;
                }
                passageCells.Add(cell);
                MotaCell[] neighbors = Neighbors(cell).ToArray();
                if (neighbors.Length != 2 || neighbors.Any(neighbor => RoomId(neighbor) >= 0)
                    || request.TeleportCells.Contains(cell))
                {
                    nodes.Add(cell);
                }
            }
        }

        HashSet<Edge> visited = [];
        foreach (MotaCell cell in passageCells.Where(nodes.Contains))
        {
            cancellationToken.ThrowIfCancellationRequested();
            MotaCell[] neighbors = Neighbors(cell).Where(neighbor => RoomId(neighbor) < 0).ToArray();
            if (neighbors.Length == 0)
            {
                AddCorridor([cell]);
            }
            foreach (MotaCell neighbor in neighbors)
            {
                FollowCorridor(cell, neighbor, nodes, visited);
            }
        }
        foreach (MotaCell cell in passageCells)
        {
            cancellationToken.ThrowIfCancellationRequested();
            foreach (MotaCell neighbor in Neighbors(cell).Where(neighbor => RoomId(neighbor) < 0))
            {
                FollowCorridor(cell, neighbor, nodes, visited);
            }
        }
    }

    private void FollowCorridor(MotaCell start, MotaCell next, HashSet<MotaCell> nodes, HashSet<Edge> visited)
    {
        if (!visited.Add(Edge.Create(start, next)))
        {
            return;
        }
        List<MotaCell> cells = [start];
        MotaCell previous = start;
        MotaCell current = next;
        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (current == start)
            {
                break;
            }
            cells.Add(current);
            if (nodes.Contains(current))
            {
                break;
            }
            MotaCell[] following = Neighbors(current)
                .Where(cell => cell != previous && RoomId(cell) < 0).ToArray();
            if (following.Length != 1 || !visited.Add(Edge.Create(current, following[0])))
            {
                break;
            }
            previous = current;
            current = following[0];
        }
        AddCorridor(cells);
    }

    private void AddCorridor(List<MotaCell> cells)
    {
        Corridor corridor = new Corridor(corridors.Count, cells);
        corridors.Add(corridor);
        AddEntrances(corridor, cells[0], false);
        if (cells.Count > 1)
        {
            AddEntrances(corridor, cells[^1], true);
        }
    }

    private void AddEntrances(Corridor corridor, MotaCell endpoint, bool reverse)
    {
        foreach (MotaCell inner in Neighbors(endpoint).Where(cell => RoomId(cell) >= 0))
        {
            Room room = rooms[RoomId(inner)];
            room.Entrances.Add(new Entrance(corridor.Id, inner, reverse));
        }
    }

    private void FindPockets()
    {
        List<Pocket> candidates = [];
        for (int y = 0; y < Height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            for (int x = 0; x < Width; x++)
            {
                MotaCell gate = new MotaCell(x, y);
                if (!IsWalkable(gate) || !IsGate(gate) || request.TeleportCells.Contains(gate))
                {
                    continue;
                }
                MotaCell[] neighbors = Neighbors(gate).ToArray();
                for (int side = 0; side < 2; side++)
                {
                    IReadOnlyList<MotaCell>? cells = CollectPocket(gate, neighbors[side], neighbors[1 - side]);
                    if (cells is not null && HasPocketAnchor(gate, neighbors[1 - side]))
                    {
                        candidates.Add(new Pocket(gate, cells));
                    }
                }
            }
        }
        candidates.Sort((left, right) =>
        {
            int size = right.Cells.Count.CompareTo(left.Cells.Count);
            if (size != 0)
            {
                return size;
            }
            int gate = CompareCells(left.Gate, right.Gate);
            return gate != 0 ? gate : CompareCells(left.Cells[0], right.Cells[0]);
        });
        HashSet<MotaCell> occupiedCells = [];
        HashSet<MotaCell> usedGates = [];
        foreach (Pocket candidate in candidates)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (occupiedCells.Contains(candidate.Gate) || usedGates.Contains(candidate.Gate)
                || candidate.Cells.Any(cell => occupiedCells.Contains(cell) || usedGates.Contains(cell)))
            {
                continue;
            }
            pockets.Add(candidate);
            occupiedCells.UnionWith(candidate.Cells);
            usedGates.Add(candidate.Gate);
        }
    }

    private IReadOnlyList<MotaCell>? CollectPocket(MotaCell gate, MotaCell first, MotaCell opposite)
    {
        HashSet<MotaCell> visited = [first];
        Queue<MotaCell> pending = new Queue<MotaCell>();
        pending.Enqueue(first);
        while (pending.TryDequeue(out MotaCell cell))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (RoomId(cell) >= 0 || request.TeleportCells.Contains(cell) || cell == opposite)
            {
                return null;
            }
            foreach (MotaCell neighbor in Neighbors(cell))
            {
                if (neighbor == gate || !visited.Add(neighbor))
                {
                    continue;
                }
                if (visited.Count > 8)
                {
                    return null;
                }
                pending.Enqueue(neighbor);
            }
        }
        List<MotaCell> cells = visited.ToList();
        cells.Sort(CompareCells);
        return cells.ToArray();
    }

    private bool HasPocketAnchor(MotaCell gate, MotaCell first)
    {
        HashSet<MotaCell> visited = [gate, first];
        Queue<MotaCell> pending = new Queue<MotaCell>();
        pending.Enqueue(first);
        while (pending.TryDequeue(out MotaCell cell))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (RoomId(cell) >= 0 || request.TeleportCells.Contains(cell))
            {
                return true;
            }
            MotaCell[] neighbors = Neighbors(cell).ToArray();
            if (neighbors.Length >= 3)
            {
                return true;
            }
            foreach (MotaCell neighbor in neighbors)
            {
                if (visited.Add(neighbor))
                {
                    pending.Enqueue(neighbor);
                }
            }
        }
        return false;
    }

    public static int CompareCells(MotaCell left, MotaCell right)
    {
        int row = left.Y.CompareTo(right.Y);
        return row != 0 ? row : left.X.CompareTo(right.X);
    }

    internal sealed record Room(int Id, List<MotaCell> Cells)
    {
        public List<Entrance> Entrances { get; } = [];
    }

    internal sealed record Corridor(int Id, List<MotaCell> Cells);
    internal sealed record Entrance(int CorridorId, MotaCell InnerCell, bool Reverse);
    internal sealed record Pocket(MotaCell Gate, IReadOnlyList<MotaCell> Cells);

    private readonly record struct Edge(MotaCell First, MotaCell Second)
    {
        public static Edge Create(MotaCell first, MotaCell second)
        {
            return CompareCells(first, second) <= 0 ? new Edge(first, second) : new Edge(second, first);
        }
    }
}
