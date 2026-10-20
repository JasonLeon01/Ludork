using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.EditorTutorial.Models;
using Ludork.EditorTutorial.Views;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.EditorTutorial.Playback;

internal sealed class TutorialPlayer : IDisposable
{
    private readonly Window window;
    private readonly Grid root;
    private readonly Control content;
    private readonly TutorialTriggerBus triggers;
    private readonly EditorSettings settings;
    private readonly Action<string> reportError;
    private readonly Dictionary<string, Func<Rect?>> anchors = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Func<CancellationToken, Task<TutorialPlaybackContext?>>> contextProviders = new(StringComparer.Ordinal);
    private readonly List<IDisposable> inputLocks = [];
    private readonly HashSet<IPointer> dismissPointers = [];
    private readonly Stopwatch clock = new();
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(16) };
    private readonly HashSet<string> failedScenes = new(StringComparer.Ordinal);
    private TutorialCatalog? catalog;
    private TutorialScene? scene;
    private TutorialOverlay? overlay;
    private KeyBinding[] keyBindings = [];
    private IInputElement? previousFocus;
    private bool dismissing;
    private bool disposed;
    private CancellationTokenSource? preparation;

    public TutorialPlayer(Window window, Grid root, Control content, TutorialTriggerBus triggers,
        EditorSettings settings, Action<string> reportError)
    {
        this.window = window;
        this.root = root;
        this.content = content;
        this.triggers = triggers;
        this.settings = settings;
        this.reportError = reportError;
        triggers.Triggered += onTriggered;
        timer.Tick += onTick;
        window.PropertyChanged += onWindowStateChanged;
        window.Closed += onClosed;
        root.LayoutUpdated += onLayoutUpdated;
        window.AddHandler(InputElement.PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel, true);
        window.AddHandler(InputElement.PointerReleasedEvent, onPointerReleased, RoutingStrategies.Tunnel, true);
        window.AddHandler(InputElement.PointerWheelChangedEvent, onWheel, RoutingStrategies.Tunnel, true);
        window.AddHandler(InputElement.KeyDownEvent, onKey, RoutingStrategies.Tunnel, true);
        window.AddHandler(InputElement.KeyUpEvent, onKey, RoutingStrategies.Tunnel, true);
        window.AddHandler(InputElement.TextInputEvent, onText, RoutingStrategies.Tunnel, true);
        window.AddHandler(InputElement.ContextRequestedEvent, onContext, RoutingStrategies.Tunnel, true);
        window.AddHandler(DragDrop.DragOverEvent, onDrag, RoutingStrategies.Tunnel, true);
        window.AddHandler(DragDrop.DropEvent, onDrag, RoutingStrategies.Tunnel, true);
    }

    public void RegisterAnchor(string key, Func<Rect?> bounds) => anchors.Add(key, bounds);

    public void RegisterContext(string trigger, Func<CancellationToken, Task<TutorialPlaybackContext?>> provider)
        => contextProviders.Add(trigger, provider);

    private async void onTriggered(object? sender, string trigger)
    {
        if (disposed || overlay is not null || !window.IsActive || window.WindowState == WindowState.Minimized)
            return;
        catalog ??= TutorialCatalog.Load(reportError);
        TutorialScene? next = catalog.GetScene(trigger);
        if (next is null || failedScenes.Contains(next.Id) || settings.HasPlayedTutorial(next.Id)
            || !anchors.TryGetValue(next.Anchor, out Func<Rect?>? resolve) || resolve() is not { } bounds
            || bounds.Width <= 0 || bounds.Height <= 0)
            return;
        cancelPreparation();
        using CancellationTokenSource request = new();
        preparation = request;
        TutorialPlaybackContext? context;
        try
        {
            context = contextProviders.TryGetValue(trigger, out Func<CancellationToken, Task<TutorialPlaybackContext?>>? provider)
                ? await provider(request.Token) : new TutorialPlaybackContext(new Dictionary<string, Bitmap>());
        }
        catch (OperationCanceledException) when (request.IsCancellationRequested)
        {
            return;
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException
            or ArgumentException or NotSupportedException)
        {
            reportError($"Tutorial '{next.Id}': {exception.Message}");
            return;
        }
        finally
        {
            if (ReferenceEquals(preparation, request))
                preparation = null;
        }
        if (context is null)
            return;
        if (request.IsCancellationRequested || disposed || overlay is not null || !window.IsActive
            || window.WindowState == WindowState.Minimized || settings.HasPlayedTutorial(next.Id)
            || resolve() is not { Width: > 0, Height: > 0 } currentBounds
            || !context.HasImagesFor(next.Elements))
        {
            context.Dispose();
            return;
        }
        TutorialOverlay candidate = new(next, context);
        try
        {
            candidate.Prepare();
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException
            or ArgumentException or NotSupportedException)
        {
            candidate.Dispose();
            failedScenes.Add(next.Id);
            reportError($"Tutorial '{next.Id}': {exception.Message}");
            return;
        }
        if (!candidate.SetFrame(currentBounds, 0))
        {
            candidate.Dispose();
            return;
        }
        scene = next;
        overlay = candidate;
        candidate.Presented += (_, _) => settings.MarkTutorialPlayed(next.Id);
        previousFocus = window.FocusManager?.GetFocusedElement();
        keyBindings = window.KeyBindings.ToArray();
        window.KeyBindings.Clear();
        lockInput(content, InputElement.IsEnabledProperty);
        NativeMenu? menu = NativeMenu.GetMenu(window);
        if (menu is not null)
            lockMenu(menu);
        NativeMenu? applicationMenu = Application.Current is { } application ? NativeMenu.GetMenu(application) : null;
        if (applicationMenu is not null && applicationMenu != menu)
            lockMenu(applicationMenu);
        root.Children.Add(candidate);
        candidate.Focus();
        clock.Restart();
        timer.Start();
    }

    private void lockInput(AvaloniaObject target, StyledProperty<bool> property)
    {
        IDisposable? token = target.SetValue(property, false, BindingPriority.Animation);
        if (token is not null)
            inputLocks.Add(token);
    }

    private void lockMenu(NativeMenu menu)
    {
        foreach (NativeMenuItem item in menu.Items.OfType<NativeMenuItem>())
        {
            lockInput(item, NativeMenuItem.IsEnabledProperty);
            if (item.Menu is not null)
                lockMenu(item.Menu);
        }
    }

    private void onTick(object? sender, EventArgs args)
    {
        if (scene is null || overlay is null || dismissing)
            return;
        if (!anchors.TryGetValue(scene.Anchor, out Func<Rect?>? resolve) || resolve() is not { } bounds
            || bounds.Width <= 0 || bounds.Height <= 0)
        {
            stop();
            return;
        }
        if (!window.IsActive || window.WindowState == WindowState.Minimized)
        {
            clock.Stop();
            return;
        }
        clock.Start();
        if (!overlay.SetFrame(bounds, clock.Elapsed.TotalSeconds))
        {
            stop();
            return;
        }
        if (!scene.Loop && clock.Elapsed.TotalSeconds >= scene.Duration)
        {
            clock.Stop();
            timer.Stop();
        }
    }

    private void onLayoutUpdated(object? sender, EventArgs args) => onTick(sender, args);

    private void onWindowStateChanged(object? sender, AvaloniaPropertyChangedEventArgs args)
    {
        if (args.Property != WindowBase.IsActiveProperty
            && args.Property != Window.WindowStateProperty)
            return;
        if (!window.IsActive || window.WindowState == WindowState.Minimized)
            cancelPreparation();
        if (overlay is null)
            return;
        if (!window.IsActive || window.WindowState == WindowState.Minimized)
        {
            clock.Stop();
            timer.Stop();
            if (dismissing)
                stop();
        }
        else if (!dismissing)
        {
            overlay.Focus();
            if (scene is { } current && (current.Loop || clock.Elapsed.TotalSeconds < current.Duration))
            {
                clock.Start();
                timer.Start();
            }
        }
    }

    private void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        cancelPreparation();
        if (overlay is null)
            return;
        args.Handled = true;
        dismissing = true;
        clock.Stop();
        timer.Stop();
        overlay.Dismiss();
        dismissPointers.Add(args.Pointer);
        args.Pointer.Capture(overlay);
    }

    private void onPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        if (overlay is null)
            return;
        args.Handled = true;
        if (!dismissing)
            return;
        PointerPointProperties properties = args.GetCurrentPoint(window).Properties;
        if (properties.IsLeftButtonPressed || properties.IsRightButtonPressed || properties.IsMiddleButtonPressed
            || properties.IsXButton1Pressed || properties.IsXButton2Pressed)
            return;
        dismissPointers.Remove(args.Pointer);
        args.Pointer.Capture(null);
        if (dismissPointers.Count == 0)
            Dispatcher.UIThread.Post(() => { if (dismissing) stop(); }, DispatcherPriority.Background);
    }

    private void onWheel(object? sender, PointerWheelEventArgs args)
    {
        cancelPreparation();
        if (overlay is not null)
            args.Handled = true;
    }

    private void onKey(object? sender, KeyEventArgs args)
    {
        cancelPreparation();
        if (overlay is not null)
            args.Handled = true;
    }

    private void onText(object? sender, TextInputEventArgs args)
    {
        cancelPreparation();
        if (overlay is not null)
            args.Handled = true;
    }

    private void onContext(object? sender, ContextRequestedEventArgs args)
    {
        cancelPreparation();
        if (overlay is not null)
            args.Handled = true;
    }

    private void onDrag(object? sender, DragEventArgs args)
    {
        cancelPreparation();
        if (overlay is null)
            return;
        args.DragEffects = DragDropEffects.None;
        args.Handled = true;
    }

    private void cancelPreparation()
    {
        CancellationTokenSource? pending = preparation;
        preparation = null;
        pending?.Cancel();
    }

    private void stop()
    {
        cancelPreparation();
        timer.Stop();
        clock.Reset();
        if (overlay is null)
            return;
        TutorialOverlay previous = overlay;
        overlay = null;
        scene = null;
        dismissing = false;
        foreach (IPointer pointer in dismissPointers)
            pointer.Capture(null);
        dismissPointers.Clear();
        root.Children.Remove(previous);
        previous.Dispose();
        foreach (IDisposable token in inputLocks)
            token.Dispose();
        inputLocks.Clear();
        window.KeyBindings.AddRange(keyBindings);
        keyBindings = [];
        if (!disposed)
            previousFocus?.Focus();
        previousFocus = null;
    }

    private void onClosed(object? sender, EventArgs args) => Dispose();

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        stop();
        triggers.Triggered -= onTriggered;
        contextProviders.Clear();
        anchors.Clear();
        timer.Tick -= onTick;
        window.PropertyChanged -= onWindowStateChanged;
        window.Closed -= onClosed;
        root.LayoutUpdated -= onLayoutUpdated;
        window.RemoveHandler(InputElement.PointerPressedEvent, onPointerPressed);
        window.RemoveHandler(InputElement.PointerReleasedEvent, onPointerReleased);
        window.RemoveHandler(InputElement.PointerWheelChangedEvent, onWheel);
        window.RemoveHandler(InputElement.KeyDownEvent, onKey);
        window.RemoveHandler(InputElement.KeyUpEvent, onKey);
        window.RemoveHandler(InputElement.TextInputEvent, onText);
        window.RemoveHandler(InputElement.ContextRequestedEvent, onContext);
        window.RemoveHandler(DragDrop.DragOverEvent, onDrag);
        window.RemoveHandler(DragDrop.DropEvent, onDrag);
    }
}
