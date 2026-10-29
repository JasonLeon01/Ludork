using System;
using System.Collections.Generic;

namespace Ludork.Controls;

internal sealed class TimelineIntervalIndex<T> where T : notnull
{
    private readonly Func<T, double> getStart;
    private readonly Func<T, double> getEnd;
    private readonly Dictionary<T, Entry> entries = [];
    private readonly List<Entry> matches = [];
    private Node? root;
    private long nextOrder;
    private readonly Comparison<Entry> compareMatches;

    public TimelineIntervalIndex(IEnumerable<T> items, Func<T, double> getStart, Func<T, double> getEnd,
        IComparer<T>? resultComparer = null)
    {
        this.getStart = getStart;
        this.getEnd = getEnd;
        compareMatches = resultComparer is null
            ? static (left, right) => left.Order.CompareTo(right.Order)
            : (left, right) => resultComparer.Compare(left.Item, right.Item);
        Reset(items);
    }

    public double MaximumEnd => root?.MaximumEnd ?? 0;

    public void Reset(IEnumerable<T> items)
    {
        entries.Clear();
        matches.Clear();
        root = null;
        nextOrder = 0;
        foreach (T item in items)
            Add(item);
    }

    public void Add(T item)
    {
        Entry entry = createEntry(item, nextOrder++);
        entries.Add(item, entry);
        root = insert(root, entry);
    }

    public bool Remove(T item)
    {
        if (!entries.Remove(item, out Entry? previous))
            return false;
        root = remove(root, previous);
        return true;
    }

    public void Update(T item)
    {
        if (!entries.TryGetValue(item, out Entry? previous))
            return;
        Entry next = createEntry(item, previous.Order);
        if (previous.Start == next.Start && previous.End == next.End)
            return;
        root = remove(root, previous);
        entries[item] = next;
        root = insert(root, next);
    }

    public void Query(double start, double end, List<T> destination)
    {
        destination.Clear();
        matches.Clear();
        if (start > end)
            return;
        query(root, start, end);
        matches.Sort(compareMatches);
        foreach (Entry entry in matches)
            destination.Add(entry.Item);
    }

    private Entry createEntry(T item, long order)
    {
        double start = getStart(item);
        double end = getEnd(item);
        start = double.IsFinite(start) ? start : 0;
        end = double.IsFinite(end) ? end : 0;
        return new Entry(item, order, start, end);
    }

    private void query(Node? node, double start, double end)
    {
        if (node is null || node.MaximumRangeEnd < start)
            return;
        query(node.Left, start, end);
        if (node.Entry.Start > end)
            return;
        if (node.Entry.RangeEnd >= start)
            matches.Add(node.Entry);
        query(node.Right, start, end);
    }

    private static Node insert(Node? node, Entry entry)
    {
        if (node is null)
            return new Node(entry);
        if (compare(entry, node.Entry) < 0)
            node.Left = insert(node.Left, entry);
        else
            node.Right = insert(node.Right, entry);
        return balance(node);
    }

    private static Node? remove(Node? node, Entry entry)
    {
        if (node is null)
            return null;
        int comparison = compare(entry, node.Entry);
        if (comparison < 0)
            node.Left = remove(node.Left, entry);
        else if (comparison > 0)
            node.Right = remove(node.Right, entry);
        else
        {
            if (node.Left is null)
                return node.Right;
            if (node.Right is null)
                return node.Left;
            Node successor = node.Right;
            while (successor.Left is not null)
                successor = successor.Left;
            node.Entry = successor.Entry;
            node.Right = remove(node.Right, successor.Entry);
        }
        return balance(node);
    }

    private static int compare(Entry left, Entry right)
    {
        int start = left.Start.CompareTo(right.Start);
        return start != 0 ? start : left.Order.CompareTo(right.Order);
    }

    private static int height(Node? node) => node?.Height ?? 0;

    private static void refresh(Node node)
    {
        node.Height = 1 + Math.Max(height(node.Left), height(node.Right));
        node.MaximumEnd = Math.Max(node.Entry.End,
            Math.Max(node.Left?.MaximumEnd ?? double.NegativeInfinity, node.Right?.MaximumEnd ?? double.NegativeInfinity));
        node.MaximumRangeEnd = Math.Max(node.Entry.RangeEnd,
            Math.Max(node.Left?.MaximumRangeEnd ?? double.NegativeInfinity, node.Right?.MaximumRangeEnd ?? double.NegativeInfinity));
    }

    private static Node balance(Node node)
    {
        refresh(node);
        int difference = height(node.Left) - height(node.Right);
        if (difference > 1)
        {
            if (height(node.Left!.Left) < height(node.Left.Right))
                node.Left = rotateLeft(node.Left);
            return rotateRight(node);
        }
        if (difference < -1)
        {
            if (height(node.Right!.Right) < height(node.Right.Left))
                node.Right = rotateRight(node.Right);
            return rotateLeft(node);
        }
        return node;
    }

    private static Node rotateLeft(Node node)
    {
        Node next = node.Right!;
        node.Right = next.Left;
        next.Left = node;
        refresh(node);
        refresh(next);
        return next;
    }

    private static Node rotateRight(Node node)
    {
        Node next = node.Left!;
        node.Left = next.Right;
        next.Right = node;
        refresh(node);
        refresh(next);
        return next;
    }

    private sealed record Entry(T Item, long Order, double Start, double End)
    {
        public double RangeEnd => Math.Max(Start, End);
    }

    private sealed class Node(Entry entry)
    {
        public Entry Entry { get; set; } = entry;
        public Node? Left { get; set; }
        public Node? Right { get; set; }
        public int Height { get; set; } = 1;
        public double MaximumEnd { get; set; } = entry.End;
        public double MaximumRangeEnd { get; set; } = entry.RangeEnd;
    }
}
