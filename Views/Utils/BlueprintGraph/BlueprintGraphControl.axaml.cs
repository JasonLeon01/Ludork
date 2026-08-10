using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using NodifyM.Avalonia.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed partial class BlueprintGraphControl : UserControl, IDisposable
{
    private readonly BlueprintGraphEditorViewModel viewModel;
    private readonly DispatcherTimer changeTimer;
    private readonly NodifyEditor editor;
    private readonly EditorZoomInput zoomInput = new();
    private Point insertionPoint;
    private PixelPoint pickerPosition;
    private bool hasInsertionPoint;
    private bool hasPendingChange;
    private bool viewportInitialized;
    private bool pickerOpen;
    private bool disposed;

    public BlueprintGraphControl()
    {
        AvaloniaXamlLoader.Load(this);
        editor = this.FindControl<NodifyEditor>("Editor")
            ?? throw new InvalidOperationException("Blueprint graph editor was not created.");
        viewModel = null!;
        changeTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(180),
        };
        changeTimer.Tick += onChangeTimer;
        Loaded += onLoaded;
        AddHandler(PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel);
        AddHandler(PointerReleasedEvent, onPointerReleased, RoutingStrategies.Tunnel);
        AddHandler(PointerWheelChangedEvent, onPointerWheelChanged, RoutingStrategies.Tunnel);
        editor.PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public BlueprintGraphControl(
        BlueprintGraphDocument document,
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        BlueprintVariableFieldBuilder fieldBuilder,
        BlueprintNodeParameterEditorFactory parameterEditorFactory,
        string assetsDirectory,
        int cellSize,
        bool isReadOnly = false) : this()
    {
        viewModel = new BlueprintGraphEditorViewModel(
            document,
            definitions,
            fieldBuilder,
            parameterEditorFactory,
            assetsDirectory,
            cellSize,
            isReadOnly);
        DataContext = viewModel;
        document.Changed += onDocumentChanged;
        viewModel.BlueprintPendingConnection.EmptyDropRequested += onEmptyDropRequested;
    }

    public event EventHandler? GraphChanged;

    public BlueprintGraphDocument Document => viewModel.Document;

    public void OrganizeLayout()
    {
        if (viewModel.IsReadOnly)
            return;
        viewModel.OrganizeLayout();
        Dispatcher.UIThread.Post(refreshOrganizedLayout, DispatcherPriority.Loaded);
    }

    public void SetReadOnly(bool value)
    {
        viewModel.SetReadOnly(value);
    }

    public void FlushPendingChanges()
    {
        if (!hasPendingChange)
            return;
        changeTimer.Stop();
        hasPendingChange = false;
        GraphChanged?.Invoke(this, EventArgs.Empty);
    }

    public void DiscardPendingChanges()
    {
        changeTimer.Stop();
        hasPendingChange = false;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        FlushPendingChanges();
        changeTimer.Stop();
        Loaded -= onLoaded;
        Document.Changed -= onDocumentChanged;
        viewModel.BlueprintPendingConnection.EmptyDropRequested -= onEmptyDropRequested;
        RemoveHandler(PointerPressedEvent, onPointerPressed);
        RemoveHandler(PointerReleasedEvent, onPointerReleased);
        RemoveHandler(PointerWheelChangedEvent, onPointerWheelChanged);
        editor.PointerTouchPadGestureMagnify -= onPointerTouchPadGestureMagnify;
        RemoveHandler(KeyDownEvent, onKeyDown);
        foreach (BlueprintGraphNodeViewModel node in viewModel.Nodes.OfType<BlueprintGraphNodeViewModel>())
        {
            foreach (BlueprintGraphPortViewModel port in node.Input
                .Concat(node.Output)
                .OfType<BlueprintGraphPortViewModel>())
            {
                port.Dispose();
            }
            node.Dispose();
        }
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        hasPendingChange = true;
        changeTimer.Stop();
        changeTimer.Start();
    }

    private void onLoaded(object? sender, RoutedEventArgs args)
    {
        if (viewportInitialized)
            return;
        viewportInitialized = true;
        Dispatcher.UIThread.Post(resetViewport);
    }

    private void resetViewport()
    {
        if (disposed)
            return;
        editor.Zoom = 1;
        editor.OffsetX = 0;
        editor.OffsetY = 0;
        editor.ViewTranslateTransform.X = 0;
        editor.ViewTranslateTransform.Y = 0;
    }

    private void refreshOrganizedLayout()
    {
        if (disposed)
            return;
        editor.InvalidateMeasure();
        editor.InvalidateArrange();
        editor.UpdateLayout();
        foreach (Connector connector in editor.GetVisualDescendants().OfType<Connector>())
            connector.UpdateAnchor();
        fitOrganizedLayout();
    }

    private void fitOrganizedLayout()
    {
        BlueprintGraphNodeViewModel[] nodes = viewModel.Nodes
            .OfType<BlueprintGraphNodeViewModel>()
            .ToArray();
        if (nodes.Length == 0 || Bounds.Width <= 0 || Bounds.Height <= 0)
            return;
        double left = nodes.Min(node => node.Location.X);
        double top = nodes.Min(node => node.Location.Y);
        double right = nodes.Max(node => node.Location.X + 320);
        double bottom = nodes.Max(node => node.Location.Y + estimateNodeHeight(node.Model));
        const double padding = 48;
        double contentWidth = Math.Max(1, right - left);
        double contentHeight = Math.Max(1, bottom - top);
        double zoom = Math.Min(
            1,
            Math.Min(
                Math.Max(0.1, (Bounds.Width - padding * 2) / contentWidth),
                Math.Max(0.1, (Bounds.Height - padding * 2) / contentHeight)));
        editor.Zoom = zoom;
        editor.OffsetX = -left + padding / zoom;
        editor.OffsetY = -top + padding / zoom;
        editor.ViewTranslateTransform.X = editor.OffsetX;
        editor.ViewTranslateTransform.Y = editor.OffsetY;
    }

    private static double estimateNodeHeight(BlueprintGraphNode node)
    {
        return 120 + Math.Max(node.Inputs.Count, 1) * 34;
    }

    private void onChangeTimer(object? sender, EventArgs args)
    {
        FlushPendingChanges();
    }

    private void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        Focus();
        updateInsertionPoint(args.GetPosition(editor));
        if (!args.GetCurrentPoint(this).Properties.IsRightButtonPressed)
            return;
        BlueprintGraphNodeViewModel? contextNode = findContextNode(args.Source);
        selectContextNode(contextNode);
        args.Handled = true;
        showContextMenu(contextNode);
    }

    private void onPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        updateInsertionPoint(args.GetPosition(editor));
    }

    private void onPointerWheelChanged(object? sender, PointerWheelEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (!EditorZoomInput.HasPrimaryModifier(args.KeyModifiers))
        {
            Vector screenTranslation =
                EditorZoomInput.GetPannedTranslation(default, args.Delta);
            double zoom = Math.Max(0.0001, editor.Zoom);
            Vector translation = new(
                editor.OffsetX + screenTranslation.X / zoom,
                editor.OffsetY + screenTranslation.Y / zoom);
            editor.OffsetX = translation.X;
            editor.OffsetY = translation.Y;
            editor.ViewTranslateTransform.X = translation.X;
            editor.ViewTranslateTransform.Y = translation.Y;
            args.Handled = true;
            return;
        }
        if (args.Delta.Y == 0)
        {
            args.Handled = true;
            return;
        }
        Point position = args.GetPosition(editor);
        double factor = args.Delta.Y > 0 ? 1.1 : 0.9;
        applyZoom(position, factor);
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        applyZoom(
            args.GetPosition(editor),
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y));
        args.Handled = true;
    }

    private void applyZoom(Point position, double factor)
    {
        editor.ZoomCenter = new RelativePoint(position, RelativeUnit.Absolute);
        editor.Zoom = EditorZoomInput.ScaleByFactor(
            editor.Zoom,
            factor,
            0.1,
            10);
    }

    private static BlueprintGraphNodeViewModel? findContextNode(object? source)
    {
        Control? sourceControl = source as Control;
        NodifyM.Avalonia.Controls.Node? nodeControl = sourceControl as NodifyM.Avalonia.Controls.Node
            ?? sourceControl?.GetVisualAncestors()
                .OfType<NodifyM.Avalonia.Controls.Node>()
                .FirstOrDefault();
        return nodeControl?.DataContext as BlueprintGraphNodeViewModel;
    }

    private void selectContextNode(BlueprintGraphNodeViewModel? node)
    {
        if (node is null || viewModel.SelectedNodes.Contains(node))
            return;
        viewModel.SelectedNodes.Clear();
        viewModel.SelectedNodes.Add(node);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (isInputControl(args.Source))
            return;
        if (args.Key == Key.Delete)
        {
            if (viewModel.IsReadOnly)
                return;
            viewModel.DeleteSelected();
            args.Handled = true;
            return;
        }
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.N)
        {
            if (viewModel.IsReadOnly)
                return;
            await addNodeAsync();
        }
        else if (args.Key == Key.C)
        {
            viewModel.CopySelected();
        }
        else if (args.Key == Key.V)
        {
            if (viewModel.IsReadOnly)
                return;
            viewModel.Paste(getDefaultInsertionPoint());
        }
        else
        {
            return;
        }
        args.Handled = true;
    }

    private void showContextMenu(BlueprintGraphNodeViewModel? contextNode)
    {
        bool isBlank = contextNode is null;
        bool isRegularNode = contextNode is not null && !contextNode.Model.IsVirtual;
        ContextMenu menu = new();
        MenuItem add = new()
        {
            Header = LocaleService.Get("ADD_NODE"),
            IsEnabled = !viewModel.IsReadOnly && isBlank && viewModel.Definitions.Count != 0,
        };
        add.Click += async (_, _) => await addNodeAsync();
        menu.Items.Add(add);

        MenuItem setStart = new()
        {
            Header = LocaleService.Get("SET_AS_START"),
            IsEnabled = viewModel.CanSetAsStart(contextNode),
        };
        setStart.Click += (_, _) =>
        {
            if (contextNode is not null)
                viewModel.SetAsStart(contextNode);
        };
        menu.Items.Add(setStart);

        MenuItem cancelStart = new()
        {
            Header = LocaleService.Get("CANCEL_START_NODE"),
            IsEnabled = viewModel.CanClearStart(contextNode),
        };
        cancelStart.Click += (_, _) =>
        {
            if (contextNode is not null)
                viewModel.ClearStart(contextNode);
        };
        menu.Items.Add(cancelStart);

        MenuItem organize = new()
        {
            Header = LocaleService.Get("ORGANIZE_GRAPH"),
            IsEnabled = !viewModel.IsReadOnly,
        };
        organize.Click += (_, _) => OrganizeLayout();
        menu.Items.Add(organize);

        menu.Items.Add(new Separator());
        MenuItem copy = new()
        {
            Header = LocaleService.Get("COPY"),
            IsEnabled = isRegularNode && viewModel.CanCopySelected,
        };
        copy.Click += (_, _) => viewModel.CopySelected();
        menu.Items.Add(copy);
        MenuItem paste = new()
        {
            Header = LocaleService.Get("PASTE"),
            IsEnabled = !viewModel.IsReadOnly && isBlank && viewModel.CanPaste,
        };
        paste.Click += (_, _) => viewModel.Paste(insertionPoint);
        menu.Items.Add(paste);
        MenuItem delete = new()
        {
            Header = LocaleService.Get("DELETE"),
            IsEnabled = !viewModel.IsReadOnly && isRegularNode && viewModel.CanDeleteSelected,
        };
        delete.Click += (_, _) => viewModel.DeleteSelected();
        menu.Items.Add(delete);
        menu.Open(editor);
    }

    private async Task addNodeAsync()
    {
        Window? owner = TopLevel.GetTopLevel(this) as Window;
        if (viewModel.IsReadOnly || owner is null || viewModel.Definitions.Count == 0 || pickerOpen)
            return;
        pickerOpen = true;
        BlueprintGraphNodeDefinition? selected = await BlueprintNodePickerWindow.ShowAsync(
            owner,
            viewModel.Definitions,
            getPickerPosition());
        pickerOpen = false;
        if (!disposed && selected is not null)
            viewModel.AddNode(selected, getDefaultInsertionPoint());
    }

    private async void onEmptyDropRequested(
        object? sender,
        BlueprintConnectionDropEventArgs args)
    {
        if (viewModel.IsReadOnly || pickerOpen)
            return;
        BlueprintGraphNodeDefinition[] compatible = viewModel.Definitions
            .Where(definition => definition.Ports.Any(port =>
                port.Direction == BlueprintGraphPortDirection.Input
                && port.Kind == args.Source.Model.Kind))
            .ToArray();
        Window? owner = TopLevel.GetTopLevel(this) as Window;
        if (owner is null || compatible.Length == 0)
            return;
        pickerOpen = true;
        BlueprintGraphNodeDefinition? selected = await BlueprintNodePickerWindow.ShowAsync(
            owner,
            compatible,
            getPickerPosition());
        pickerOpen = false;
        if (!disposed && selected is not null)
            viewModel.AddNodeAndConnect(selected, getDefaultInsertionPoint(), args.Source);
    }

    private Point getDefaultInsertionPoint()
    {
        if (hasInsertionPoint)
            return insertionPoint;
        return toGraphPoint(new Point(editor.Bounds.Width / 2, editor.Bounds.Height / 2));
    }

    private PixelPoint getPickerPosition()
    {
        if (hasInsertionPoint)
            return pickerPosition;
        return editor.PointToScreen(new Point(editor.Bounds.Width / 2, editor.Bounds.Height / 2));
    }

    private void updateInsertionPoint(Point viewportPoint)
    {
        insertionPoint = toGraphPoint(viewportPoint);
        pickerPosition = editor.PointToScreen(viewportPoint);
        hasInsertionPoint = true;
    }

    private Point toGraphPoint(Point viewportPoint)
    {
        double zoom = Math.Abs(editor.Zoom) < double.Epsilon ? 1 : editor.Zoom;
        return new Point(
            viewportPoint.X / zoom - editor.OffsetX,
            viewportPoint.Y / zoom - editor.OffsetY);
    }

    private static bool isInputControl(object? source)
    {
        Control? control = source as Control;
        return isInputControlType(control)
            || control?.GetVisualAncestors().OfType<Control>().Any(isInputControlType) == true;
    }

    private static bool isInputControlType(Control? control)
    {
        return control is TextBox
            or NumericUpDown
            or ComboBox
            or CheckBox
            or Slider
            or SelectingItemsControl;
    }
}
