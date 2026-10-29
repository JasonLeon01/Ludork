using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Models;
using Ludork.ViewModels.BlueprintGraph;
using System;
using System.ComponentModel;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintGraphParameterEditor : ContentControl
{
    private BlueprintGraphPortViewModel? port;
    private BlueprintVariableForm? form;
    private bool attached;
    private BlueprintGraphControl? graph;
    private NodifyM.Avalonia.Controls.Node? node;
    private Size measuredSize;
    private bool measured;
    private bool suspended;
    private bool awaitingMeasurement;

    public BlueprintGraphParameterEditor()
    {
        DataContextChanged += (_, _) => bind();
        SizeChanged += (_, _) => graph?.QueueParameterEditor(this);
    }

    internal bool HasMeasuredSize => measured;

    internal void CaptureMeasuredSize()
    {
        if (form is null || !IsMeasureValid || !IsArrangeValid || Bounds.Width <= 0 || Bounds.Height <= 0)
            return;
        measuredSize = Bounds.Size;
        measured = true;
        stopAwaitingMeasurement();
    }

    internal void EnsureRealized()
    {
        suspended = false;
        ensureForm();
    }

    internal bool TrySuspend()
    {
        if (form is null)
            return suspended;
        CaptureMeasuredSize();
        bool visible = port?.IsEditorVisible == true;
        if (visible && !measured || !form.CanSuspend)
            return false;
        stopAwaitingMeasurement();
        form.SuspensionStateChanged -= onSuspensionStateChanged;
        form.Dispose();
        form = null;
        suspended = true;
        Content = visible ? new Border { Width = measuredSize.Width, Height = measuredSize.Height } : null;
        return true;
    }

    internal void InvalidateMeasuredSize()
    {
        measured = false;
        InvalidateMeasure();
        awaitMeasurement();
        graph?.QueueParameterEditor(this);
    }

    private void awaitMeasurement()
    {
        if (awaitingMeasurement || form is null)
            return;
        awaitingMeasurement = true;
        LayoutUpdated += onMeasuredLayout;
    }

    private void stopAwaitingMeasurement()
    {
        if (!awaitingMeasurement)
            return;
        awaitingMeasurement = false;
        LayoutUpdated -= onMeasuredLayout;
    }

    private void onMeasuredLayout(object? sender, EventArgs args)
    {
        CaptureMeasuredSize();
        if (measured)
            graph?.QueueParameterEditor(this);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        graph = this.FindAncestorOfType<BlueprintGraphControl>();
        node = this.FindAncestorOfType<NodifyM.Avalonia.Controls.Node>();
        if (node is not null)
            node.PropertyChanged += onNodePlacementChanged;
        bind();
        graph?.RegisterParameterEditor(this);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
        if (node is not null)
            node.PropertyChanged -= onNodePlacementChanged;
        node = null;
        graph?.UnregisterParameterEditor(this);
        graph = null;
        release();
        base.OnDetachedFromVisualTree(args);
    }

    private void bind()
    {
        if (!attached || ReferenceEquals(port, DataContext))
            return;
        release();
        if (DataContext is not BlueprintGraphPortViewModel model || model.IsDisposed)
            return;
        port = model;
        port.PropertyChanged += onStateChanged;
        port.ExternalValueChanged += onExternalValueChanged;
        port.DependenciesChanged += onDependenciesChanged;
        port.Disposed += onDisposed;
        ensureForm();
        graph?.QueueParameterEditor(this);
    }

    private void ensureForm()
    {
        if (form is not null || suspended || port is not { IsEditorVisible: true } model
            || graph?.ParameterContext is not BlueprintGraphParameterContext context)
            return;
        BlueprintVariableForm editor = new()
        {
            AssetsDirectory = context.AssetsDirectory,
            ProjectDirectory = Path.GetDirectoryName(context.AssetsDirectory) ?? string.Empty,
            CellSize = context.CellSize,
            GameVariables = context.Variables,
            EnumService = context.Data.Enums,
            HistoryGameData = context.Data,
            IsReadOnly = model.IsReadOnly,
            ShowFieldNames = false,
            MinWidth = 180,
            MaxWidth = 280,
        };
        editor.CustomValueEditorFactory = request => context.Editors.Create(request,
            model.GetRawSiblingValue, model.SetRawSiblingValue, () => !model.IsDisposed);
        editor.PlainTextEditorFactory = model.UsePlainTextInputs ? createPlainTextEditor : null;
        editor.PointerPressed += (_, args) => args.Handled = true;
        form = editor;
        editor.SuspensionStateChanged += onSuspensionStateChanged;
        editor.SetFields([model.ParameterField]);
        synchronizeDependencies();
        editor.ValueChanged += (_, args) =>
        {
            model.CommitValue(args.Value);
            if (args.RequiresRefresh)
                Dispatcher.UIThread.Post(() => { if (ReferenceEquals(form, editor)) editor.RefreshEditors(); }, DispatcherPriority.Background);
        };
        Content = editor;
        awaitMeasurement();
        graph?.QueueParameterEditor(this);
    }

    private Control createPlainTextEditor(BlueprintVariableEditorRequest request)
    {
        BlueprintGraphPortViewModel model = port!;
        string type = request.Field.UseJsonTableEditor ? model.Model.TypeName : request.Field.Type;
        BlueprintParameterTextDraft draft = model.PrepareTextDraft(type, request.Value);
        TextBox input = EditorInputs.CreateEditableTextBox(draft.Text);
        IBrush? normalBorder = input.BorderBrush;
        void showError()
        {
            input.BorderBrush = model.TextDraft?.Error is null ? normalBorder : Brushes.OrangeRed;
            ToolTip.SetTip(input, model.TextDraft?.Error);
        }
        showError();
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty || model.IsDisposed || model.IsReadOnly)
                return;
            bool valid = model.UpdateTextDraft(type, input.Text ?? string.Empty, out JsonNode? value);
            showError();
            if (valid)
                request.Commit(value, false);
        };
        return input;
    }

    private void onStateChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(BlueprintGraphPortViewModel.IsEditorVisible)
            && port is { IsEditorVisible: false })
        {
            stopAwaitingMeasurement();
            TrySuspend();
            graph?.QueueParameterEditor(this);
            return;
        }
        if (args.PropertyName is nameof(BlueprintGraphPortViewModel.UsePlainTextInputs)
            or nameof(BlueprintGraphPortViewModel.IsEditorVisible)
            or nameof(BlueprintGraphPortViewModel.DisplayTitle))
            InvalidateMeasuredSize();
        bool existing = form is not null;
        ensureForm();
        if (form is null || port is null)
            return;
        if (args.PropertyName == nameof(BlueprintGraphPortViewModel.IsReadOnly))
            form.IsReadOnly = port.IsReadOnly;
        else if (existing && args.PropertyName == nameof(BlueprintGraphPortViewModel.UsePlainTextInputs))
        {
            form.PlainTextEditorFactory = port.UsePlainTextInputs ? createPlainTextEditor : null;
            form.RefreshEditors();
        }
    }

    private void onExternalValueChanged(object? sender, EventArgs args)
    {
        InvalidateMeasuredSize();
        if (port is not null)
            form?.SetFieldValue(port.Model.Name, port.Model.Value);
    }

    private void onDependenciesChanged(object? sender, EventArgs args)
    {
        InvalidateMeasuredSize();
        synchronizeDependencies();
    }

    private void onSuspensionStateChanged(object? sender, EventArgs args)
        => graph?.QueueParameterEditor(this);

    private void onNodePlacementChanged(object? sender, AvaloniaPropertyChangedEventArgs args)
    {
        if (args.Property.Name == "Location")
            graph?.QueueParameterEditor(this);
    }

    private void synchronizeDependencies()
    {
        if (port is null || form is null)
            return;
        foreach (BlueprintGraphPortViewModel dependency in port.Dependencies)
            form.SetDependencyValue(dependency.Model.Name, dependency.Model.IsConnected ? null : dependency.Model.Value);
    }

    private void onDisposed(object? sender, EventArgs args) => release();

    private void release()
    {
        stopAwaitingMeasurement();
        if (port is not null)
        {
            port.PropertyChanged -= onStateChanged;
            port.ExternalValueChanged -= onExternalValueChanged;
            port.DependenciesChanged -= onDependenciesChanged;
            port.Disposed -= onDisposed;
        }
        if (form is not null)
        {
            form.SuspensionStateChanged -= onSuspensionStateChanged;
            form.Dispose();
        }
        form = null;
        port = null;
        suspended = false;
        measured = false;
        Content = null;
    }
}
