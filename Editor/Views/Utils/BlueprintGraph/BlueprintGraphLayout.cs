using Avalonia;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views.Utils.BlueprintGraph;

internal static class BlueprintGraphLayout
{
    private const double HorizontalSpacing = 720;
    private const double VerticalSpacing = 320;
    private const double EventParameterSpacing = 64;
    private const double EventParameterStartY = 64;
    private const double StartGap = 250;
    private const double ColumnPadding = 24;

    public static IReadOnlyDictionary<Guid, Point> Compute(BlueprintGraphDocument document)
    {
        BlueprintGraphNode[] regularNodes = document.Nodes
            .Where(node => !node.IsVirtual)
            .OrderBy(node => node.OriginalIndex ?? int.MaxValue)
            .ThenBy(node => node.Title, StringComparer.Ordinal)
            .ToArray();
        BlueprintGraphNode[] virtualNodes = document.Nodes
            .Where(node => node.IsVirtual)
            .OrderBy(node => node.ExternalKey, StringComparer.Ordinal)
            .ToArray();
        Dictionary<Guid, BlueprintGraphNode> regularById = regularNodes
            .ToDictionary(node => node.Id);
        Dictionary<Guid, Point> positions = [];
        for (int index = 0; index < virtualNodes.Length; index++)
        {
            positions[virtualNodes[index].Id] = new Point(
                0,
                EventParameterStartY + index * EventParameterSpacing);
        }

        double startY = virtualNodes.Length == 0
            ? 0
            : EventParameterStartY
                + (virtualNodes.Length - 1) * EventParameterSpacing
                + StartGap;
        Dictionary<Guid, List<Guid>> execChildren = buildExecChildren(document, regularById);
        Dictionary<Guid, List<Guid?>> parameterProviders = buildParameterProviders(document, regularById);
        Guid? startId = document.Start?.NodeId is Guid candidate && regularById.ContainsKey(candidate)
            ? candidate
            : null;
        if (startId is Guid resolvedStart)
        {
            Dictionary<Guid, int> depths = computeExecDepths(
                resolvedStart,
                execChildren,
                regularNodes.Length);
            HashSet<Guid> visited = [];
            assignExecPositions(
                resolvedStart,
                startY,
                execChildren,
                depths,
                regularById,
                positions,
                visited);
        }
        else
        {
            double orphanY = startY;
            foreach (BlueprintGraphNode node in regularNodes)
            {
                positions[node.Id] = new Point(0, orphanY);
                orphanY += VerticalSpacing;
            }
        }

        int nextLane = layoutParameterProviders(
            regularNodes,
            parameterProviders,
            positions,
            startY);
        int parameterLaneCount = computeParameterLaneCount(
            regularNodes,
            parameterProviders,
            positions,
            startY,
            nextLane);
        layoutRemainingNodes(regularNodes, positions, startY, parameterLaneCount);
        resolveColumnOverlaps(regularNodes, positions);
        return positions;
    }

    private static Dictionary<Guid, List<Guid>> buildExecChildren(
        BlueprintGraphDocument document,
        IReadOnlyDictionary<Guid, BlueprintGraphNode> regularById)
    {
        Dictionary<Guid, List<Guid>> children = [];
        HashSet<(Guid Source, Guid Target)> seen = [];
        foreach (BlueprintGraphConnection connection in document.Connections)
        {
            if (connection.Kind != BlueprintGraphPortKind.Exec
                || connection.Source.NodeId is not Guid sourceId
                || connection.Target.NodeId is not Guid targetId
                || !regularById.ContainsKey(sourceId)
                || !regularById.ContainsKey(targetId)
                || !seen.Add((sourceId, targetId)))
            {
                continue;
            }
            if (!children.TryGetValue(sourceId, out List<Guid>? targets))
            {
                targets = [];
                children[sourceId] = targets;
            }
            targets.Add(targetId);
        }
        return children;
    }

    private static Dictionary<Guid, List<Guid?>> buildParameterProviders(
        BlueprintGraphDocument document,
        IReadOnlyDictionary<Guid, BlueprintGraphNode> regularById)
    {
        Dictionary<Guid, List<BlueprintGraphConnection>> links = [];
        foreach (BlueprintGraphConnection connection in document.Connections)
        {
            if (connection.Kind != BlueprintGraphPortKind.Params
                || connection.Target.NodeId is not Guid targetId
                || !regularById.ContainsKey(targetId))
            {
                continue;
            }
            if (!links.TryGetValue(targetId, out List<BlueprintGraphConnection>? targetLinks))
            {
                targetLinks = [];
                links[targetId] = targetLinks;
            }
            targetLinks.Add(connection);
        }
        return links.ToDictionary(
            pair => pair.Key,
            pair => pair.Value
                .OrderBy(connection => connection.TargetPinIndex)
                .ThenBy(connection => connection.OriginalIndex ?? int.MaxValue)
                .Select(connection => connection.Source.NodeId is Guid sourceId
                    && regularById.ContainsKey(sourceId)
                        ? sourceId
                        : (Guid?)null)
                .ToList());
    }

    private static Dictionary<Guid, int> computeExecDepths(
        Guid startId,
        IReadOnlyDictionary<Guid, List<Guid>> execChildren,
        int nodeCount)
    {
        Dictionary<Guid, int> depths = new() { [startId] = 0 };
        for (int iteration = 0; iteration < nodeCount; iteration++)
        {
            bool changed = false;
            foreach (KeyValuePair<Guid, List<Guid>> pair in execChildren)
            {
                if (!depths.TryGetValue(pair.Key, out int sourceDepth))
                    continue;
                foreach (Guid targetId in pair.Value)
                {
                    int nextDepth = Math.Min(nodeCount - 1, sourceDepth + 1);
                    if (depths.TryGetValue(targetId, out int targetDepth)
                        && targetDepth >= nextDepth)
                    {
                        continue;
                    }
                    depths[targetId] = nextDepth;
                    changed = true;
                }
            }
            if (!changed)
                break;
        }
        return depths;
    }

    private static void assignExecPositions(
        Guid nodeId,
        double y,
        IReadOnlyDictionary<Guid, List<Guid>> execChildren,
        IReadOnlyDictionary<Guid, int> depths,
        IReadOnlyDictionary<Guid, BlueprintGraphNode> regularById,
        IDictionary<Guid, Point> positions,
        ISet<Guid> visited)
    {
        if (!regularById.ContainsKey(nodeId) || !visited.Add(nodeId))
            return;
        int depth = depths.TryGetValue(nodeId, out int value) ? value : 0;
        positions[nodeId] = new Point(depth * HorizontalSpacing, y);
        if (!execChildren.TryGetValue(nodeId, out List<Guid>? children))
            return;
        if (children.Count == 1)
        {
            assignExecPositions(
                children[0],
                y,
                execChildren,
                depths,
                regularById,
                positions,
                visited);
            return;
        }
        double branchY = y - (children.Count - 1) * VerticalSpacing / 2;
        for (int index = 0; index < children.Count; index++)
        {
            assignExecPositions(
                children[index],
                branchY + index * VerticalSpacing,
                execChildren,
                depths,
                regularById,
                positions,
                visited);
        }
    }

    private static int layoutParameterProviders(
        IReadOnlyList<BlueprintGraphNode> regularNodes,
        IReadOnlyDictionary<Guid, List<Guid?>> parameterProviders,
        IDictionary<Guid, Point> positions,
        double startY)
    {
        BlueprintGraphNode[] positioned = regularNodes
            .Where(node => positions.ContainsKey(node.Id))
            .OrderBy(node => positions[node.Id].X)
            .ThenBy(node => positions[node.Id].Y)
            .ToArray();
        Dictionary<Guid, double> laneByConsumer = [];
        int nextLane = 0;
        foreach (BlueprintGraphNode node in positioned)
        {
            if (!parameterProviders.TryGetValue(node.Id, out List<Guid?>? providers)
                || providers.Count == 0)
            {
                continue;
            }
            nextLane++;
            laneByConsumer[node.Id] = startY + nextLane * VerticalSpacing;
        }
        foreach (Guid consumerId in laneByConsumer.Keys
            .OrderBy(id => positions[id].X)
            .ToArray())
        {
            double laneY = laneByConsumer[consumerId];
            bool firstProvider = true;
            foreach (Guid? providerId in parameterProviders[consumerId])
            {
                if (providerId is not Guid resolvedProviderId)
                    continue;
                if (!firstProvider)
                {
                    nextLane++;
                    laneY = startY + nextLane * VerticalSpacing;
                }
                HashSet<Guid> path = [];
                ensureParameterProvider(
                    resolvedProviderId,
                    consumerId,
                    laneY,
                    parameterProviders,
                    positions,
                    ref nextLane,
                    startY,
                    path);
                firstProvider = false;
            }
        }
        return nextLane;
    }

    private static void ensureParameterProvider(
        Guid providerId,
        Guid consumerId,
        double laneY,
        IReadOnlyDictionary<Guid, List<Guid?>> parameterProviders,
        IDictionary<Guid, Point> positions,
        ref int nextLane,
        double startY,
        ISet<Guid> path)
    {
        if (!positions.TryGetValue(consumerId, out Point consumerPosition)
            || !path.Add(providerId))
        {
            return;
        }
        double targetX = consumerPosition.X - HorizontalSpacing;
        positions[providerId] = positions.TryGetValue(providerId, out Point providerPosition)
            ? new Point(Math.Min(providerPosition.X, targetX), providerPosition.Y)
            : new Point(targetX, laneY);
        if (parameterProviders.TryGetValue(providerId, out List<Guid?>? providers))
        {
            bool firstProvider = true;
            double childLaneY = laneY;
            foreach (Guid? childId in providers)
            {
                if (childId is not Guid resolvedChildId)
                    continue;
                if (!firstProvider)
                {
                    nextLane++;
                    childLaneY = startY + nextLane * VerticalSpacing;
                }
                ensureParameterProvider(
                    resolvedChildId,
                    providerId,
                    childLaneY,
                    parameterProviders,
                    positions,
                    ref nextLane,
                    startY,
                    path);
                firstProvider = false;
            }
        }
        path.Remove(providerId);
    }

    private static int computeParameterLaneCount(
        IReadOnlyList<BlueprintGraphNode> regularNodes,
        IReadOnlyDictionary<Guid, List<Guid?>> parameterProviders,
        IReadOnlyDictionary<Guid, Point> positions,
        double startY,
        int nextLane)
    {
        int laneCount = regularNodes.Count(node =>
            positions.ContainsKey(node.Id)
            && parameterProviders.TryGetValue(node.Id, out List<Guid?>? providers)
            && providers.Count != 0);
        laneCount = Math.Max(laneCount, nextLane);
        foreach (BlueprintGraphNode node in regularNodes)
        {
            if (!positions.TryGetValue(node.Id, out Point position) || position.Y <= startY + 1)
                continue;
            int lane = (int)Math.Round((position.Y - startY) / VerticalSpacing);
            laneCount = Math.Max(laneCount, lane);
        }
        return laneCount;
    }

    private static void layoutRemainingNodes(
        IReadOnlyList<BlueprintGraphNode> regularNodes,
        IDictionary<Guid, Point> positions,
        double startY,
        int parameterLaneCount)
    {
        double maxX = regularNodes
            .Where(node => positions.ContainsKey(node.Id))
            .Select(node => positions[node.Id].X)
            .DefaultIfEmpty(0)
            .Max();
        double orphanY = startY + (parameterLaneCount + 1) * VerticalSpacing;
        foreach (BlueprintGraphNode node in regularNodes)
        {
            if (positions.ContainsKey(node.Id))
                continue;
            positions[node.Id] = new Point(maxX + HorizontalSpacing, orphanY);
            orphanY += VerticalSpacing;
        }
    }

    private static void resolveColumnOverlaps(
        IReadOnlyList<BlueprintGraphNode> regularNodes,
        IDictionary<Guid, Point> positions)
    {
        BlueprintGraphNode[] ordered = regularNodes
            .Where(node => positions.ContainsKey(node.Id))
            .OrderBy(node => positions[node.Id].X)
            .ThenBy(node => positions[node.Id].Y)
            .ToArray();
        int start = 0;
        while (start < ordered.Length)
        {
            double columnX = positions[ordered[start].Id].X;
            int end = start + 1;
            while (end < ordered.Length
                && Math.Abs(positions[ordered[end].Id].X - columnX) < 1)
            {
                end++;
            }
            for (int index = start + 1; index < end; index++)
            {
                BlueprintGraphNode upper = ordered[index - 1];
                BlueprintGraphNode lower = ordered[index];
                double minimumY = positions[upper.Id].Y
                    + estimateNodeHeight(upper)
                    + ColumnPadding;
                if (positions[lower.Id].Y < minimumY)
                    positions[lower.Id] = new Point(positions[lower.Id].X, minimumY);
            }
            start = end;
        }
    }

    private static double estimateNodeHeight(BlueprintGraphNode node)
    {
        return 120 + Math.Max(node.Inputs.Count, 1) * 34;
    }
}
