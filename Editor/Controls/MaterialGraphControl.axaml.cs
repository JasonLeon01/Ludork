using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.ViewModels.Materials;
using Ludork.Views.Utils;
using NodifyM.Avalonia.Controls;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MaterialGraphControl : UserControl
{
    private readonly NodifyEditor editor;
    private readonly EditorZoomInput zoomInput = new();
    private Point pointer;
    private long gesture;
    private bool pointerDown;
    private bool restoringView;
    private bool viewportInitialized;
    private string? pendingFocus;
    private bool rightPressed;
    private bool rightDragged;
    private MaterialGraphNodeViewModel? rightNode;
    private MaterialGraphPortViewModel? rightPort;
    private MaterialGraphNodeViewModel[] rightSelection = [];
    private bool anchorRefreshPending;
    private bool completingGesture;
    private InputElement? capturedElement;
    private readonly DispatcherTimer viewGestureTimer = new() { Interval = TimeSpan.FromMilliseconds(300) };

    public MaterialGraphControl()
    {
        AvaloniaXamlLoader.Load(this);
        editor = this.FindControl<NodifyEditor>("Editor")!;
        AddHandler(PointerPressedEvent, (_, args) =>
        {
            if (IsInputEditor(args.Source as Visual))
                return;
            Focus();
            endGesture();
            pointerDown = true;
            pointer = args.GetPosition(editor);
            rightPressed = args.GetCurrentPoint(editor).Properties.PointerUpdateKind == PointerUpdateKind.RightButtonPressed;
            rightDragged = false;
            if (rightPressed && DataContext is MaterialGraphViewModel model)
            {
                rightNode = findNode(args.Source as Visual);
                rightPort = findPort(args.Source as Visual);
                rightSelection = model.SelectedNodes.OfType<MaterialGraphNodeViewModel>().ToArray();
            }
            if (GameData is not null)
                gesture = GameData.BeginHistoryGesture(flushLayout);
        }, RoutingStrategies.Tunnel);
        AddHandler(PointerPressedEvent, (_, args) =>
        {
            if (pointerDown)
                trackCapture(args.Pointer.Captured as InputElement);
        }, RoutingStrategies.Bubble, true);
        AddHandler(PointerMovedEvent, (_, args) =>
        {
            if (!pointerDown && IsInputEditor(args.Source as Visual))
                return;
            Point position = args.GetPosition(editor);
            double deltaX = position.X - pointer.X;
            double deltaY = position.Y - pointer.Y;
            if (rightPressed && deltaX * deltaX + deltaY * deltaY > 16)
                rightDragged = true;
        }, RoutingStrategies.Tunnel, true);
        AddHandler(PointerReleasedEvent, (_, args) =>
        {
            if (!pointerDown && IsInputEditor(args.Source as Visual))
                return;
            pointerDown = false;
            trackCapture(null);
            endGesture();
            if (args.InitialPressMouseButton == MouseButton.Right && rightPressed)
            {
                rightPressed = false;
                if (!rightDragged && DataContext is MaterialGraphViewModel model)
                {
                    model.SelectedNodes.Clear();
                    foreach (MaterialGraphNodeViewModel node in rightSelection)
                        if (model.FindNode(node.Id) == node)
                            model.SelectedNodes.Add(node);
                    if (rightPort is not null)
                        showPortContextMenu(rightPort, true);
                    else
                        showContextMenu(rightNode, true);
                }
                rightNode = null;
                rightPort = null;
                rightSelection = [];
            }
            SelectionChanged?.Invoke(this, EventArgs.Empty);
        }, RoutingStrategies.Bubble, true);
        AddHandler(ContextRequestedEvent, onContextRequested, RoutingStrategies.Tunnel);
        AddHandler(PointerWheelChangedEvent, onPointerWheelChanged, RoutingStrategies.Tunnel);
        editor.PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        viewGestureTimer.Tick += (_, _) => endGesture();
        DetachedFromVisualTree += (_, _) =>
        {
            viewportInitialized = false;
            trackCapture(null);
            pointerDown = rightPressed = false;
            rightNode = null;
            rightPort = null;
            rightSelection = [];
            endGesture();
        };
        DataContextChanged += (_, _) => RestoreLayout();
        Loaded += (_, _) => Dispatcher.UIThread.Post(() =>
        {
            viewportInitialized = true;
            restoreView();
            RequestAnchorRefresh();
            if (pendingFocus is string id)
            {
                pendingFocus = null;
                FocusNode(id);
            }
        });
        editor.PropertyChanged += (_, args) =>
        {
            if (!viewportInitialized || restoringView || args.Property.Name is not ("Zoom" or "OffsetX" or "OffsetY") || DataContext is not MaterialGraphViewModel model)
                return;
            model.SetView(editor.Zoom, editor.OffsetX, editor.OffsetY);
        };
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    private void onPointerWheelChanged(object? sender, PointerWheelEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS || IsInputEditor(args.Source as Visual))
            return;
        pointer = args.GetPosition(editor);
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (!EditorZoomInput.HasPrimaryModifier(args.KeyModifiers))
        {
            Vector translation = EditorZoomInput.GetPanTranslation(args.Delta) / Math.Max(0.0001, editor.Zoom);
            editor.OffsetX += translation.X;
            editor.OffsetY += translation.Y;
            editor.ViewTranslateTransform.X = editor.OffsetX;
            editor.ViewTranslateTransform.Y = editor.OffsetY;
        }
        else if (args.Delta.Y != 0)
        {
            applyZoom(pointer, args.Delta.Y > 0 ? 1.1 : 0.9);
        }
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(object? sender, PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        pointer = args.GetPosition(editor);
        zoomInput.MarkMagnify();
        applyZoom(pointer, EditorZoomInput.GetMagnifyFactor(args.Delta.Y));
        args.Handled = true;
    }

    private void applyZoom(Point position, double factor)
    {
        editor.ZoomCenter = new RelativePoint(position, RelativeUnit.Absolute);
        editor.Zoom = EditorZoomInput.ScaleByFactor(editor.Zoom, factor, 0.1, 10);
    }

    private void trackCapture(InputElement? element)
    {
        if (ReferenceEquals(capturedElement, element))
            return;
        capturedElement?.RemoveHandler(PointerCaptureLostEvent, onPointerCaptureLost);
        capturedElement = element;
        capturedElement?.AddHandler(PointerCaptureLostEvent, onPointerCaptureLost, RoutingStrategies.Direct, true);
    }

    private void onPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        if (args.Pointer.Captured is InputElement next
            && (ReferenceEquals(next, this) || next.GetVisualAncestors().Contains(this)))
        {
            trackCapture(next);
            return;
        }
        trackCapture(null);
        pointerDown = false;
        endGesture();
    }

    private void endGesture()
    {
        if (completingGesture)
            return;
        completingGesture = true;
        try
        {
            flushLayout();
        }
        finally
        {
            GameData?.EndHistoryGesture(gesture);
            gesture = 0;
            completingGesture = false;
        }
    }

    private void flushLayout()
    {
        viewGestureTimer.Stop();
        LayoutCommitRequested?.Invoke(this, EventArgs.Empty);
    }

    public void ScheduleLayoutCommit()
    {
        EnsureLayoutGesture();
        if (!pointerDown)
        {
            viewGestureTimer.Stop();
            viewGestureTimer.Start();
        }
    }

    internal void EnsureLayoutGesture()
    {
        if (GameData is not null && !GameData.IsHistoryGestureActive(gesture))
            gesture = GameData.BeginHistoryGesture(flushLayout);
    }

    public void CompleteLayoutGesture() => endGesture();

    public void CancelLayoutGesture()
    {
        trackCapture(null);
        viewGestureTimer.Stop();
        GameData?.EndHistoryGesture(gesture);
        gesture = 0;
        pointerDown = rightPressed = false;
        rightNode = null;
        rightPort = null;
        rightSelection = [];
    }

    public void RestoreLayout()
    {
        CancelLayoutGesture();
        restoreView();
        RequestAnchorRefresh();
    }

    private void restoreView()
    {
        if (!viewportInitialized || DataContext is not MaterialGraphViewModel model)
            return;
        restoringView = true;
        (double zoom, double x, double y) = model.View;
        editor.Zoom = zoom;
        editor.OffsetX = x;
        editor.OffsetY = y;
        editor.ViewTranslateTransform.X = x;
        editor.ViewTranslateTransform.Y = y;
        restoringView = false;
    }

    public ProjectDataStore? GameData { get; set; }
    public event EventHandler? SelectionChanged;
    public event EventHandler? LayoutCommitRequested;
    public MaterialGraphViewModel ViewModel => (MaterialGraphViewModel)DataContext!;
    public Point InsertionPoint => new(pointer.X / Math.Max(editor.Zoom, 0.001) - editor.OffsetX,
        pointer.Y / Math.Max(editor.Zoom, 0.001) - editor.OffsetY);

    public void RequestAnchorRefresh()
    {
        if (anchorRefreshPending)
            return;
        anchorRefreshPending = true;
        Dispatcher.UIThread.Post(() =>
        {
            if (viewportInitialized)
            {
                editor.UpdateLayout();
                foreach (Connector connector in editor.GetVisualDescendants().OfType<Connector>())
                    connector.UpdateAnchor();
            }
            anchorRefreshPending = false;
        }, DispatcherPriority.Loaded);
    }

    public void FocusNode(string id)
    {
        MaterialGraphNodeViewModel? node = ViewModel.FindNode(id);
        if (node is null)
            return;
        ViewModel.SelectedNodes.Clear();
        ViewModel.SelectedNodes.Add(node);
        if (!viewportInitialized)
        {
            pendingFocus = id;
            SelectionChanged?.Invoke(this, EventArgs.Empty);
            return;
        }
        editor.OffsetX = 80 - node.Location.X;
        editor.OffsetY = 80 - node.Location.Y;
        editor.ViewTranslateTransform.X = editor.OffsetX;
        editor.ViewTranslateTransform.Y = editor.OffsetY;
        SelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    private static MaterialGraphNodeViewModel? findNode(Visual? source)
    {
        for (Visual? current = source; current is not null; current = current.GetVisualParent())
            if (current is Node { DataContext: MaterialGraphNodeViewModel node })
                return node;
        return null;
    }

    private static MaterialGraphPortViewModel? findPort(Visual? source)
    {
        for (Visual? current = source; current is not null; current = current.GetVisualParent())
            if (current is Connector { DataContext: MaterialGraphPortViewModel port })
                return port;
        return null;
    }

    internal static bool IsInputEditor(Visual? source)
    {
        for (Visual? current = source; current is not null; current = current.GetVisualParent())
            if (current is MaterialGraphInlineEditor or TextBox or NumericUpDown or ComboBox)
                return true;
        return false;
    }

    private void onContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        if (DataContext is not MaterialGraphViewModel model || IsInputEditor(args.Source as Visual))
            return;
        bool requestedByPointer = args.TryGetPosition(editor, out Point position);
        args.Handled = true;
        if (requestedByPointer)
        {
            if (!EditorContextMenuBehavior.IsMixedButtonRequest(args))
                return;
            pointer = position;
        }
        if (findPort(args.Source as Visual) is MaterialGraphPortViewModel port)
            showPortContextMenu(port, requestedByPointer);
        else
            showContextMenu(requestedByPointer ? findNode(args.Source as Visual) : model.SelectedNode, requestedByPointer);
    }

    private void selectContextNode(MaterialGraphNodeViewModel? node)
    {
        if (node is not null && !ViewModel.SelectedNodes.Contains(node))
        {
            ViewModel.SelectedNodes.Clear();
            ViewModel.SelectedNodes.Add(node);
        }
    }

    private MenuItem actionItem(string label, bool enabled, Action action)
    {
        MenuItem item = new() { Header = LocaleService.Get(label), IsEnabled = enabled };
        item.Click += (_, _) =>
        {
            action();
            SelectionChanged?.Invoke(this, EventArgs.Empty);
        };
        return item;
    }

    private void showPortContextMenu(MaterialGraphPortViewModel port, bool requestedByPointer)
    {
        if (DataContext is not MaterialGraphViewModel model)
            return;
        selectContextNode(model.FindNode(port.NodeId));
        ContextMenu menu = new() { Placement = requestedByPointer ? PlacementMode.Pointer : PlacementMode.Bottom };
        menu.Items.Add(actionItem(port.IsInput ? "MATERIAL_DISCONNECT_INPUT" : "MATERIAL_DISCONNECT_OUTPUT",
            model.CanDisconnectPort(port), () => model.DisconnectConnector(port)));
        if (model.CanSetConstant(port))
            menu.Items.Add(actionItem("MATERIAL_SET_CONSTANT", true, () => model.SetConstant(port)));
        if (model.CanResetInput(port))
            menu.Items.Add(actionItem("MATERIAL_RESET_INPUT", true, () => model.ResetInput(port)));
        SelectionChanged?.Invoke(this, EventArgs.Empty);
        menu.Open(this);
    }

    private void showContextMenu(MaterialGraphNodeViewModel? node, bool requestedByPointer)
    {
        if (DataContext is not MaterialGraphViewModel model)
            return;
        ContextMenu menu = new() { Placement = requestedByPointer ? PlacementMode.Pointer : PlacementMode.Bottom };
        if (node is not null)
        {
            selectContextNode(node);
            menu.Items.Add(actionItem("DISCONNECT_ALL_INPUTS", model.CanDisconnectNode(node, true), () => model.DisconnectNode(node, true)));
            menu.Items.Add(actionItem("DISCONNECT_ALL_OUTPUTS", model.CanDisconnectNode(node, false), () => model.DisconnectNode(node, false)));
            menu.Items.Add(new Separator());
            menu.Items.Add(actionItem("DELETE", model.SelectedNodes.OfType<MaterialGraphNodeViewModel>().Any(selected => selected.Type != "Surface"),
                model.DeleteSelected));
        }
        else
        {
            Point position = InsertionPoint;
            menu.Items.Add(new MenuItem
            {
                Header = new TextBlock { Text = LocaleService.Get("ADD_MATERIAL_NODE"), FontWeight = Avalonia.Media.FontWeight.SemiBold },
                IsEnabled = false,
            });
            menu.Items.Add(new Separator());
            foreach (IGrouping<string, JsonObject> category in model.NodeDescriptors
                .Where(descriptor => descriptor["type"]!.GetValue<string>() != "Surface")
                .GroupBy(descriptor => descriptor["category"]!.GetValue<string>()))
            {
                MenuItem group = new() { Header = category.Key };
                foreach (JsonObject descriptor in category)
                {
                    string type = descriptor["type"]!.GetValue<string>();
                    MenuItem item = new() { Header = descriptor["displayName"]?.GetValue<string>() ?? type };
                    item.Click += (_, _) =>
                    {
                        model.AddNode(type, position);
                        SelectionChanged?.Invoke(this, EventArgs.Empty);
                    };
                    group.Items.Add(item);
                }
                menu.Items.Add(group);
            }
        }
        SelectionChanged?.Invoke(this, EventArgs.Empty);
        menu.Open(this);
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (DataContext is not MaterialGraphViewModel graph || IsInputEditor(args.Source as Visual))
            return;
        bool command = (args.KeyModifiers & (OperatingSystem.IsMacOS() ? KeyModifiers.Meta : KeyModifiers.Control)) != 0;
        if (args.Key == Key.Delete || args.Key == Key.Back)
            graph.DeleteSelected();
        else if (command && args.Key == Key.C)
            graph.Copy();
        else if (command && args.Key == Key.V)
            graph.Paste(InsertionPoint);
        else if (command && args.Key == Key.A)
        {
            graph.SelectedNodes.Clear();
            foreach (MaterialGraphNodeViewModel node in graph.Nodes.OfType<MaterialGraphNodeViewModel>())
                graph.SelectedNodes.Add(node);
        }
        else
            return;
        args.Handled = true;
        SelectionChanged?.Invoke(this, EventArgs.Empty);
    }
}
