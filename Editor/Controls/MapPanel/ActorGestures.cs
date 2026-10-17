using Avalonia;
using Avalonia.Input;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private ActorPointerGesture? actorPointerGesture;
    private readonly List<MapActorPasteEntry> actorGroupClipboard = [];
    private (int X, int Y) actorClipboardAnchor;

    private sealed class ActorPointerGesture(
        Point start, (int X, int Y) grid, KeyModifiers modifiers, MapActorReference hit,
        bool wasSelected, IReadOnlyList<MapActorPositionEdit> positions)
    {
        public Point Start { get; } = start;
        public (int X, int Y) Grid { get; } = grid;
        public KeyModifiers Modifiers { get; } = modifiers;
        public MapActorReference Hit { get; } = hit;
        public bool WasSelected { get; } = wasSelected;
        public IReadOnlyList<MapActorPositionEdit> Positions { get; } = positions;
        public bool Dragging { get; set; }
        public bool Ignored { get; set; }
    }

    private void handleProjectActorPointerPressed(PointerPressedEventArgs args, (int X, int Y) grid)
    {
        PointerPoint point = args.GetCurrentPoint(this);
        if (selectedLayerName is null || !point.Properties.IsLeftButtonPressed || point.Properties.IsRightButtonPressed)
            return;
        int? hitIndex = getLayer(selectedLayerName) is MapLayerSnapshot layer && isLayerVisible(layer)
            && getMapDisplayPosition(point.Position, out _, out _) is { } mapPosition
            ? hitTestActor(selectedLayerName, mapPosition) : null;
        MapActorSnapshot? hit = hitIndex is int index ? getActorList(selectedLayerName)?[index] : null;
        bool modified = args.KeyModifiers.HasFlag(KeyModifiers.Shift)
            || args.KeyModifiers.HasFlag(KeyModifiers.Alt) || EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        if (hit is null)
        {
            if (!modified)
            {
                if (selectedActors.Count > 1)
                    setProjectActorSelection([], null, null);
                else if (canEditMap && !string.IsNullOrWhiteSpace(pendingActor))
                    placeActor(pendingActor, grid);
                else
                    setProjectActorSelection([], null, null);
            }
            args.Handled = true;
            InvalidateVisual();
            return;
        }
        MapActorReference reference = new(selectedLayerName, hit.Tag);
        bool wasSelected = selectedActors.Contains(reference);
        if (!modified && !wasSelected)
            setProjectActorSelection([reference], reference, reference);
        List<MapActorPositionEdit> positions = [];
        foreach (MapActorReference selected in selectedActors)
        {
            if (findProjectActor(selected) is int selectedIndex
                && getActorList(selected.LayerName)?[selectedIndex] is MapActorSnapshot actor
                && actor.TryGetGridPosition(out int x, out int y))
                positions.Add(new(selected, x, y));
        }
        actorPointerGesture = new(point.Position, grid, args.KeyModifiers, reference, wasSelected, positions);
        capturePointer(args.Pointer);
        args.Handled = true;
        InvalidateVisual();
    }

    private void updateProjectActorGesture(PointerEventArgs args)
    {
        if (actorPointerGesture is not ActorPointerGesture gesture
            || !args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Point position = args.GetPosition(this);
        if (tryGetMapSize(out int hoverWidth, out int hoverHeight))
            hoverGrid = getGridPosition(position, hoverWidth, hoverHeight);
        Vector delta = position - gesture.Start;
        if (!gesture.Dragging)
        {
            if (Math.Abs(delta.X) < 4 && Math.Abs(delta.Y) < 4)
                return;
            gesture.Dragging = true;
            bool modified = gesture.Modifiers.HasFlag(KeyModifiers.Shift)
                || gesture.Modifiers.HasFlag(KeyModifiers.Alt)
                || EditorShortcuts.HasPrimaryModifier(gesture.Modifiers);
            gesture.Ignored = !canEditMap || modified && !gesture.WasSelected;
            if (!gesture.Ignored && (gesture.Modifiers.HasFlag(KeyModifiers.Alt)
                || EditorShortcuts.HasPrimaryModifier(gesture.Modifiers)))
                gesture.Ignored = !beginActorPropertyDrag(gesture.Start, gesture.Modifiers);
        }
        args.Handled = true;
        if (gesture.Ignored)
            return;
        if (actorPropertyDrag is not null)
        {
            updateActorPropertyDrag(position);
            return;
        }
        if (gameData is null || CurrentMapKey is null || gesture.Positions.Count == 0
            || !tryGetMapSize(out int width, out int height))
            return;
        Rect map = getMapRect(width, height);
        int dx = (int)Math.Floor((position.X - map.X) / tileSize) - gesture.Grid.X;
        int dy = (int)Math.Floor((position.Y - map.Y) / tileSize) - gesture.Grid.Y;
        int minimumX = gesture.Positions.Min(actor => actor.X);
        int maximumX = gesture.Positions.Max(actor => actor.X);
        int minimumY = gesture.Positions.Min(actor => actor.Y);
        int maximumY = gesture.Positions.Max(actor => actor.Y);
        if (gesture.Positions.Count != selectedActors.Count || (long)maximumX - minimumX >= width
            || (long)maximumY - minimumY >= height)
        {
            gesture.Ignored = true;
            showEditFeedback("ACTOR_GROUP_MOVE_FAILED");
            return;
        }
        dx = Math.Clamp(dx, -minimumX, width - 1 - maximumX);
        dy = Math.Clamp(dy, -minimumY, height - 1 - maximumY);
        MapActorPositionEdit[] edits = gesture.Positions.Select(actor => actor with
        {
            X = actor.X + dx,
            Y = actor.Y + dy,
        }).ToArray();
        ensurePropertyHistoryGesture();
        gameData.Maps.MoveMapActors(CurrentMapKey, edits);
    }

    private void completeProjectActorGesture()
    {
        if (actorPointerGesture is not ActorPointerGesture gesture || gesture.Dragging)
            return;
        MapActorReference reference = gesture.Hit;
        if (gesture.Modifiers.HasFlag(KeyModifiers.Alt))
            return;
        if (gesture.Modifiers.HasFlag(KeyModifiers.Shift))
            setProjectActorSelection(selectedActors.Append(reference).ToArray(), reference, ActorSelectionAnchor ?? reference);
        else if (EditorShortcuts.HasPrimaryModifier(gesture.Modifiers))
        {
            MapActorReference[] next = selectedActors.Contains(reference)
                ? selectedActors.Where(actor => actor != reference).ToArray()
                : selectedActors.Append(reference).ToArray();
            setProjectActorSelection(next, reference, reference);
        }
    }

    public void CopySelectedActors()
    {
        actorGroupClipboard.Clear();
        if (CurrentMapDocument is null)
            return;
        if (IsRuntimeEditing)
        {
            if (getSelectedActor() is MapActorSnapshot runtimeActor && runtimeActor.TryGetGridPosition(out int x, out int y))
            {
                actorGroupClipboard.Add(new(runtimeActor.ToJson(), CurrentMapDocument.ReadActorOverrides(runtimeActor.Tag),
                    x, y));
                actorClipboardAnchor = (x, y);
            }
            return;
        }
        foreach (MapActorReference reference in selectedActors)
        {
            if (findProjectActor(reference) is not int index || getActorList(reference.LayerName)?[index] is not MapActorSnapshot actor
                || !actor.TryGetGridPosition(out int x, out int y))
                continue;
            actorGroupClipboard.Add(new(actor.ToJson(), CurrentMapDocument.ReadActorOverrides(actor.Tag), x, y));
            if (reference == primarySelectedActor)
                actorClipboardAnchor = (x, y);
        }
    }

    public void DeleteSelectedActors()
    {
        if (IsRuntimeEditing)
        {
            deleteSelectedActor();
            return;
        }
        if (!canEditMap || gameData is null || CurrentMapKey is null || selectedActors.Count == 0)
            return;
        CancelInteractions();
        if (gameData.Maps.DeleteMapActors(CurrentMapKey, selectedActors.ToArray()))
            setProjectActorSelection([], null, null);
    }

    private void pasteSelectedActors((int X, int Y) grid)
    {
        if (IsRuntimeEditing || !canEditMap || gameData is null || CurrentMapKey is null
            || selectedLayerName is null || actorGroupClipboard.Count == 0)
            return;
        CancelInteractions();
        MapActorPasteEntry[] entries = actorGroupClipboard.Select(actor => actor with
        {
            X = grid.X + actor.X - actorClipboardAnchor.X,
            Y = grid.Y + actor.Y - actorClipboardAnchor.Y,
        }).ToArray();
        IReadOnlyList<MapActorReference>? pasted = gameData.Maps.PasteMapActors(CurrentMapKey, selectedLayerName, entries);
        if (pasted is null)
            showEditFeedback("ACTOR_BATCH_PASTE_FAILED");
        else
        {
            int primaryIndex = actorGroupClipboard.FindIndex(actor => actor.X == actorClipboardAnchor.X && actor.Y == actorClipboardAnchor.Y);
            MapActorReference? primary = primaryIndex >= 0 && primaryIndex < pasted.Count ? pasted[primaryIndex] : pasted.LastOrDefault();
            setProjectActorSelection(pasted, primary, primary);
        }
        InvalidateVisual();
    }
}
