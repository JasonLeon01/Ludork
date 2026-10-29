using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using NodifyM.Avalonia.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed partial class BlueprintGraphControl
{
    private readonly HashSet<BlueprintGraphParameterEditor> parameterEditors = [];
    private readonly HashSet<BlueprintGraphParameterEditor> pendingParameterEditors = [];
    private readonly CancellationTokenSource visualLifetime = new();
    private bool parameterRefreshScheduled;
    private bool allParametersPending;
    private bool visualsInitialized;
    private Task? initializationTask;
    private Window? graphWindow;
    private long graphGesture;
    private IPointer? graphPointer;

    public Task InitializeVisualsAsync(CancellationToken cancellationToken)
    {
        Task initialization = initializationTask ??= initializeVisualsAsync(cancellationToken);
        return initialization.WaitAsync(cancellationToken);
    }

    private async Task initializeVisualsAsync(CancellationToken cancellationToken)
    {
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, visualLifetime.Token);
        await viewModel.InitializeAsync(prepareConnectionsAsync, cancellation.Token);
        cancellation.Token.ThrowIfCancellationRequested();
        visualsInitialized = true;
        initializeViewport();
        if (pendingViewState is ViewState state)
        {
            pendingViewState = null;
            RestoreViewState(state);
        }
        editor.IsEnabled = true;
        queueAllParameterEditors();
    }

    private async Task prepareConnectionsAsync(CancellationToken cancellationToken)
    {
        await EditorUiBatch.YieldAsync(cancellationToken);
        editor.UpdateLayout();
        EditorUiBatch batch = new();
        foreach (Connector connector in editor.GetVisualDescendants().OfType<Connector>().ToArray())
        {
            connector.UpdateAnchor();
            await batch.YieldIfNeededAsync(cancellationToken);
        }
        foreach (BlueprintGraphParameterEditor parameter in parameterEditors.ToArray())
        {
            refreshParameterEditor(parameter);
            await batch.YieldIfNeededAsync(cancellationToken);
        }
    }

    internal void RegisterParameterEditor(BlueprintGraphParameterEditor parameter)
    {
        parameterEditors.Add(parameter);
        QueueParameterEditor(parameter);
    }

    internal void UnregisterParameterEditor(BlueprintGraphParameterEditor parameter)
    {
        parameterEditors.Remove(parameter);
        pendingParameterEditors.Remove(parameter);
    }

    internal void QueueParameterEditor(BlueprintGraphParameterEditor parameter)
    {
        if (disposed)
            return;
        pendingParameterEditors.Add(parameter);
        scheduleParameterRefresh();
    }

    private void scheduleParameterRefresh()
    {
        if (parameterRefreshScheduled)
            return;
        parameterRefreshScheduled = true;
        Dispatcher.UIThread.Post(refreshParameterEditors, DispatcherPriority.Background);
    }

    private void queueAllParameterEditors()
    {
        if (disposed)
            return;
        allParametersPending = true;
        scheduleParameterRefresh();
    }

    private async void refreshParameterEditors()
    {
        if (disposed)
        {
            parameterRefreshScheduled = false;
            return;
        }
        CancellationToken token = visualLifetime.Token;
        try
        {
            EditorUiBatch batch = new();
            while (!disposed && (allParametersPending || pendingParameterEditors.Count != 0))
            {
                BlueprintGraphParameterEditor[] pending = allParametersPending
                    ? parameterEditors.ToArray() : pendingParameterEditors.ToArray();
                allParametersPending = false;
                pendingParameterEditors.Clear();
                foreach (BlueprintGraphParameterEditor parameter in pending)
                {
                    token.ThrowIfCancellationRequested();
                    if (parameterEditors.Contains(parameter))
                        refreshParameterEditor(parameter);
                    await batch.YieldIfNeededAsync(token);
                }
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        finally
        {
            parameterRefreshScheduled = false;
        }
    }

    private void refreshParameterEditor(BlueprintGraphParameterEditor parameter)
    {
        if (!IsVisible || !parameter.IsVisible || editor.Bounds.Width <= 0 || editor.Bounds.Height <= 0)
        {
            parameter.TrySuspend();
            return;
        }
        Matrix? transform = parameter.TransformToVisual(editor);
        if (transform is null)
            return;
        Rect bounds = new Rect(parameter.Bounds.Size).TransformToAABB(transform.Value);
        Rect viewport = new Rect(editor.Bounds.Size).Inflate(256 / Math.Max(0.0001, editor.Zoom));
        if (viewport.Intersects(bounds) || !parameter.HasMeasuredSize)
            parameter.EnsureRealized();
        parameter.CaptureMeasuredSize();
        if (!viewport.Intersects(bounds))
            parameter.TrySuspend();
    }

    private void onViewportPropertyChanged(object? sender, AvaloniaPropertyChangedEventArgs args)
    {
        if (args.Property.Name is "Zoom" or "OffsetX" or "OffsetY" or "Bounds")
            queueAllParameterEditors();
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (parameterEditors is null)
            return;
        if (change.Property.Name is "ActualThemeVariant" or "FontFamily" or "FontSize")
        {
            foreach (BlueprintGraphParameterEditor parameter in parameterEditors)
                parameter.InvalidateMeasuredSize();
        }
        else if (change.Property == IsVisibleProperty)
            queueAllParameterEditors();
    }

    private void onGraphSizeChanged(object? sender, SizeChangedEventArgs args)
        => queueAllParameterEditors();

    private void scheduleFinishGraphGesture()
    {
        long version = graphPointerVersion;
        Dispatcher.UIThread.Post(() =>
        {
            if (!disposed && graphPointer is not null && version == graphPointerVersion)
            {
                graphPointer = null;
                FlushPendingChanges();
            }
        }, DispatcherPriority.Input);
    }

    private void onGraphPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        if (ReferenceEquals(graphPointer, args.Pointer))
            scheduleFinishGraphGesture();
    }

    private void onGraphDeactivated(object? sender, EventArgs args)
    {
        graphPointer = null;
        FlushPendingChanges();
    }

    private async void initializeLoadedGraph()
    {
        if (initializationTask is not null || disposed)
            return;
        try
        {
            await InitializeVisualsAsync(visualLifetime.Token);
        }
        catch (OperationCanceledException) when (disposed)
        {
        }
        catch (Exception error)
        {
            if (disposed)
                return;
            TextBlock message = this.FindControl<TextBlock>("SearchError")!;
            message.Text = error.Message;
            message.IsVisible = true;
        }
    }
}
