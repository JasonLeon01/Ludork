using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private readonly HashSet<Control> transientInputs = [];
    private readonly HashSet<IPointer> interactionPointers = [];
    private int activeInteractions;

    internal event EventHandler? SuspensionStateChanged;

    internal bool HasTransientInput => transientInputs.Count != 0
        || nestedHistoryForms.Any(nested => nested.HasTransientInput);

    internal bool CanSuspend => !disposed && !building && !IsKeyboardFocusWithin
        && !HasTransientInput && activeInteractions == 0
        && !interactionPointers.Any(pointer => pointer.Captured is Visual captured
            && (ReferenceEquals(captured, this) || this.IsVisualAncestorOf(captured)))
        && nestedHistoryForms.All(nested => nested.CanSuspend)
        && !this.GetVisualDescendants().OfType<Control>().Any(control =>
            control.ContextMenu?.IsOpen == true
            || control is ComboBox { IsDropDownOpen: true }
            || control is Button { Flyout.IsOpen: true }
            || control is Popup { IsOpen: true });

    internal IDisposable BeginInteraction()
    {
        activeInteractions++;
        notifySuspensionStateChanged();
        return new InteractionScope(this);
    }

    internal static IDisposable? BeginInteraction(Control source)
        => source.FindAncestorOfType<BlueprintVariableForm>()?.BeginInteraction();

    private void initializeSuspension()
    {
        AddHandler(PointerPressedEvent, onInteractionPointerPressed, RoutingStrategies.Tunnel, true);
        AddHandler(PointerReleasedEvent, onInteractionPointerReleased, RoutingStrategies.Bubble, true);
        PointerCaptureLost += (_, args) =>
        {
            interactionPointers.Remove(args.Pointer);
            notifySuspensionStateChanged();
        };
        LostFocus += (_, _) => Dispatcher.UIThread.Post(notifySuspensionStateChanged, DispatcherPriority.Background);
    }

    private void onInteractionPointerPressed(object? sender, PointerPressedEventArgs args)
        => interactionPointers.Add(args.Pointer);

    private void onInteractionPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        interactionPointers.Remove(args.Pointer);
        Dispatcher.UIThread.Post(notifySuspensionStateChanged, DispatcherPriority.Background);
    }

    private void setTransientInput(Control input, bool transient)
    {
        bool changed = transient ? transientInputs.Add(input) : transientInputs.Remove(input);
        if (changed)
        {
            input.DetachedFromVisualTree -= onTransientInputDetached;
            if (transient)
                input.DetachedFromVisualTree += onTransientInputDetached;
            notifySuspensionStateChanged();
        }
    }

    private void onTransientInputDetached(object? sender, VisualTreeAttachmentEventArgs args)
    {
        if (sender is Control input)
            setTransientInput(input, false);
    }

    private void trackNumericInput(NumericUpDown input)
    {
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != NumericUpDown.TextProperty && args.Property != NumericUpDown.ValueProperty)
                return;
            string text = input.Text ?? string.Empty;
            bool matches = string.IsNullOrWhiteSpace(text) ? input.Value is null
                : decimal.TryParse(text, NumberStyles.Float | NumberStyles.AllowThousands, input.NumberFormat,
                    out decimal parsed) && parsed == input.Value;
            setTransientInput(input, !matches);
        };
    }

    private void onNestedSuspensionStateChanged(object? sender, EventArgs args)
        => notifySuspensionStateChanged();

    private void notifySuspensionStateChanged()
    {
        if (!disposed)
            SuspensionStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private sealed class InteractionScope(BlueprintVariableForm owner) : IDisposable
    {
        private BlueprintVariableForm? form = owner;

        public void Dispose()
        {
            BlueprintVariableForm? current = form;
            form = null;
            if (current is null)
                return;
            current.activeInteractions--;
            current.notifySuspensionStateChanged();
        }
    }
}
