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

    public BlueprintGraphParameterEditor() => DataContextChanged += (_, _) => bind();

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        bind();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
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
    }

    private void ensureForm()
    {
        if (form is not null || port is not { IsEditorVisible: true } model
            || this.FindAncestorOfType<BlueprintGraphControl>()?.ParameterContext is not BlueprintGraphParameterContext context)
            return;
        BlueprintVariableForm editor = new()
        {
            AssetsDirectory = context.AssetsDirectory,
            ProjectDirectory = Path.GetDirectoryName(context.AssetsDirectory) ?? string.Empty,
            CellSize = context.CellSize,
            GameVariables = context.Variables,
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
        editor.SetFields([model.ParameterField]);
        synchronizeDependencies();
        editor.ValueChanged += (_, args) =>
        {
            model.CommitValue(args.Value);
            if (args.RequiresRefresh)
                Dispatcher.UIThread.Post(() => { if (ReferenceEquals(form, editor)) editor.RefreshEditors(); }, DispatcherPriority.Background);
        };
        Content = editor;
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
        if (port is not null)
            form?.SetFieldValue(port.Model.Name, port.Model.Value);
    }

    private void onDependenciesChanged(object? sender, EventArgs args) => synchronizeDependencies();

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
        if (port is not null)
        {
            port.PropertyChanged -= onStateChanged;
            port.ExternalValueChanged -= onExternalValueChanged;
            port.DependenciesChanged -= onDependenciesChanged;
            port.Disposed -= onDisposed;
        }
        form?.Dispose();
        form = null;
        port = null;
        Content = null;
    }
}
