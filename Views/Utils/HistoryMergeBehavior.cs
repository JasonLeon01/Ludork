using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Services;
using System;
using System.Runtime.CompilerServices;

namespace Ludork.Views.Utils;

public static class HistoryMergeBehavior
{
    private static readonly ConditionalWeakTable<GameDataService, GestureScope> scopes = new();
    private static readonly ConditionalWeakTable<Control, ControlAttachment> attachedControls = new();
    private static readonly ConditionalWeakTable<Control, BoundaryAttachment> attachedBoundaries = new();

    public static void Attach(TextBox control, GameDataService gameData)
    {
        attach(control, gameData, changed => control.TextChanged += (_, _) => changed());
    }

    public static void Attach(NumericUpDown control, GameDataService gameData)
    {
        attach(control, gameData, changed => control.ValueChanged += (_, _) => changed());
    }

    public static void AttachBoundary(Control boundary, GameDataService gameData)
    {
        if (attachedBoundaries.TryGetValue(boundary, out BoundaryAttachment? existing))
        {
            existing.GameData = gameData;
            return;
        }
        BoundaryAttachment attachment = new(gameData);
        attachedBoundaries.Add(boundary, attachment);
        boundary.AddHandler(
            InputElement.PointerPressedEvent,
            (_, args) => endOutsideActiveControl(args, attachment.GameData),
            RoutingStrategies.Tunnel);
    }

    private static void attach(
        Control control,
        GameDataService gameData,
        Action<Action> subscribeChange)
    {
        if (attachedControls.TryGetValue(control, out ControlAttachment? existing))
        {
            existing.Rebind(gameData);
            existing.StartIfFocused();
            return;
        }
        ControlAttachment attachment = new(control, gameData);
        attachedControls.Add(control, attachment);
        control.GotFocus += (_, _) => attachment.HandleGotFocus();
        control.LostFocus += (_, _) => attachment.HandleLostFocus();
        subscribeChange(attachment.HandleChanged);
        attachment.StartIfFocused();
    }

    private static void endOutsideActiveControl(
        PointerPressedEventArgs args,
        GameDataService gameData)
    {
        GestureScope scope = scopes.GetOrCreateValue(gameData);
        if (scope.ActiveControl is null || args.Source is not Visual source
            || isInside(source, scope.ActiveControl))
        {
            return;
        }
        gameData.EndHistoryGesture(scope.GestureId);
        scope.ActiveControl = null;
        scope.GestureId = 0;
    }

    private static bool isInside(Visual source, Control target)
    {
        Visual? current = source;
        while (current is not null)
        {
            if (ReferenceEquals(current, target))
                return true;
            current = current.GetVisualParent();
        }
        return false;
    }

    private sealed class ControlAttachment
    {
        private readonly Control control;
        private GameDataService gameData;
        private long gestureId;

        public ControlAttachment(Control control, GameDataService gameData)
        {
            this.control = control;
            this.gameData = gameData;
        }

        public void Rebind(GameDataService nextGameData)
        {
            if (ReferenceEquals(gameData, nextGameData))
                return;
            endGesture();
            gameData = nextGameData;
            StartIfFocused();
        }

        public void HandleGotFocus()
        {
            if (!gameData.IsHistoryGestureActive(gestureId))
                startGesture();
        }

        public void HandleLostFocus()
        {
            if (!control.IsKeyboardFocusWithin)
                endGesture();
        }

        public void HandleChanged()
        {
            if (control.IsKeyboardFocusWithin && !gameData.IsHistoryGestureActive(gestureId))
                startGesture();
        }

        public void StartIfFocused()
        {
            if (control.IsKeyboardFocusWithin && !gameData.IsHistoryGestureActive(gestureId))
                startGesture();
        }

        private void startGesture()
        {
            gestureId = gameData.BeginHistoryGesture();
            GestureScope scope = scopes.GetOrCreateValue(gameData);
            scope.ActiveControl = control;
            scope.GestureId = gestureId;
        }

        private void endGesture()
        {
            gameData.EndHistoryGesture(gestureId);
            GestureScope scope = scopes.GetOrCreateValue(gameData);
            if (scope.GestureId == gestureId)
            {
                scope.ActiveControl = null;
                scope.GestureId = 0;
            }
            gestureId = 0;
        }
    }

    private sealed class BoundaryAttachment(GameDataService gameData)
    {
        public GameDataService GameData { get; set; } = gameData;
    }

    private sealed class GestureScope
    {
        public Control? ActiveControl { get; set; }
        public long GestureId { get; set; }
    }
}
