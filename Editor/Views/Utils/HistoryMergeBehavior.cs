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
    private static readonly ConditionalWeakTable<ProjectDataStore, GestureScope> scopes = new();
    private static readonly ConditionalWeakTable<Control, ControlAttachment> attachedControls = new();
    private static readonly ConditionalWeakTable<Control, BoundaryAttachment> attachedBoundaries = new();

    public static void Attach(TextBox control, ProjectDataStore gameData)
    {
        attach(control, gameData, changed => control.TextChanged += (_, _) => changed());
    }

    public static void Attach(NumericUpDown control, ProjectDataStore gameData)
    {
        attach(control, gameData, changed => control.ValueChanged += (_, _) => changed());
    }

    public static void AttachFocused(Control control, ProjectDataStore gameData)
    {
        if (control is NumericUpDown number)
            Attach(number, gameData);
        else if (control is TextBox text)
            Attach(text, gameData);
        if (attachedControls.TryGetValue(control, out ControlAttachment? attachment))
            attachment.HandleGotFocus();
    }

    public static void AttachBoundary(Control boundary, ProjectDataStore gameData)
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

    public static void Detach(Control control)
    {
        if (attachedControls.TryGetValue(control, out ControlAttachment? attachment))
            attachment.Rebind(null);
    }

    public static void DetachBoundary(Control boundary)
    {
        if (attachedBoundaries.TryGetValue(boundary, out BoundaryAttachment? attachment))
            attachment.GameData = null;
    }

    private static void attach(
        Control control,
        ProjectDataStore gameData,
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
        control.AddHandler(InputElement.TextInputEvent, (_, _) => attachment.StartIfFocused(), RoutingStrategies.Tunnel);
        control.AddHandler(InputElement.KeyDownEvent, (_, _) => attachment.StartIfFocused(), RoutingStrategies.Tunnel);
        control.AddHandler(InputElement.PointerPressedEvent, (_, _) => attachment.HandleGotFocus(), RoutingStrategies.Tunnel);
        control.AddHandler(InputElement.PointerWheelChangedEvent, (_, _) => attachment.StartIfFocused(), RoutingStrategies.Tunnel);
        subscribeChange(attachment.HandleChanged);
        attachment.StartIfFocused();
    }

    private static void endOutsideActiveControl(
        PointerPressedEventArgs args,
        ProjectDataStore? gameData)
    {
        if (gameData is null)
            return;
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
        private ProjectDataStore? gameData;
        private long gestureId;

        public ControlAttachment(Control control, ProjectDataStore gameData)
        {
            this.control = control;
            this.gameData = gameData;
        }

        public void Rebind(ProjectDataStore? nextGameData)
        {
            if (ReferenceEquals(gameData, nextGameData))
                return;
            endGesture();
            gameData = nextGameData;
            StartIfFocused();
        }

        public void HandleGotFocus()
        {
            if (gameData is not null && !gameData.IsHistoryGestureActive(gestureId))
                startGesture();
        }

        public void HandleLostFocus()
        {
            if (!control.IsKeyboardFocusWithin)
                endGesture();
        }

        public void HandleChanged()
        {
            if (gameData is not null && control.IsKeyboardFocusWithin && !gameData.IsHistoryGestureActive(gestureId))
                startGesture();
        }

        public void StartIfFocused()
        {
            if (gameData is not null && control.IsKeyboardFocusWithin && !gameData.IsHistoryGestureActive(gestureId))
                startGesture();
        }

        private void startGesture()
        {
            if (gameData is null)
                return;
            gestureId = gameData.BeginHistoryGesture();
            GestureScope scope = scopes.GetOrCreateValue(gameData);
            scope.ActiveControl = control;
            scope.GestureId = gestureId;
        }

        private void endGesture()
        {
            if (gameData is null)
                return;
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

    private sealed class BoundaryAttachment(ProjectDataStore gameData)
    {
        public ProjectDataStore? GameData { get; set; } = gameData;
    }

    private sealed class GestureScope
    {
        public Control? ActiveControl { get; set; }
        public long GestureId { get; set; }
    }
}
