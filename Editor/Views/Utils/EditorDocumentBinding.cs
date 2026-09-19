using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Services;
using System;
using System.Linq;

namespace Ludork.Views.Utils;

public sealed class EditorDocumentBinding : IDisposable
{
    private readonly Window owner;
    private readonly GameDataService gameData;
    private readonly Func<EditorDocument?> resolve;
    private readonly Func<string> title;
    private readonly Action? refresh;
    private readonly bool closeWhenDeleted;
    private EditorDocument? document;
    private bool disposed;

    public EditorDocumentBinding(
        Window owner,
        GameDataService gameData,
        Func<EditorDocument?> resolve,
        Func<string> title,
        Action? refresh = null,
        bool closeWhenDeleted = false)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.resolve = resolve;
        this.title = title;
        this.refresh = refresh;
        this.closeWhenDeleted = closeWhenDeleted;
        gameData.Documents.ContentChanged += onRegistryChanged;
        owner.Closed += onClosed;
        owner.Activated += onWindowActivated;
        owner.Deactivated += onWindowActivityChanged;
        owner.AddHandler(InputElement.GotFocusEvent, onGotFocus, RoutingStrategies.Bubble);
        HistoryMergeBehavior.AttachBoundary(owner, gameData);
        Refresh();
    }

    public EditorDocument? Document => document;
    public Func<bool>? HasPendingInputs { get; set; }

    public void Refresh()
    {
        EditorDocument? next = resolve();
        if (!ReferenceEquals(document, next))
        {
            if (document is not null)
                document.Changed -= onDocumentChanged;
            document = next;
            if (document is not null)
                document.Changed += onDocumentChanged;
            gameData.BreakHistoryGesture();
        }
        updateTitle();
    }

    public HistoryResult Undo()
    {
        Refresh();
        return document is null ? new HistoryResult(false) : gameData.Undo(document.Section, document.Key);
    }

    public HistoryResult Redo()
    {
        Refresh();
        return document is null ? new HistoryResult(false) : gameData.Redo(document.Section, document.Key);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.Documents.ContentChanged -= onRegistryChanged;
        owner.Closed -= onClosed;
        owner.Activated -= onWindowActivated;
        owner.Deactivated -= onWindowActivityChanged;
        owner.RemoveHandler(InputElement.GotFocusEvent, onGotFocus);
        if (document is not null)
            document.Changed -= onDocumentChanged;
    }

    private void updateTitle()
    {
        owner.Title = (document?.IsModified == true || HasPendingInputs?.Invoke() == true ? "* " : string.Empty) + title();
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (closeWhenDeleted && document is { Exists: false })
        {
            owner.Close();
            return;
        }
        updateTitle();
    }

    private void onRegistryChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (disposed)
            return;
        if (args.Reset || document is null || args.Changes.Any(change =>
                change.DocumentId == document.Id && change.IdentityChanged))
            Refresh();
        if (args.Reset || document is not null && args.Changes.Any(change => change.DocumentId == document.Id))
            refresh?.Invoke();
    }

    private void onWindowActivityChanged(object? sender, EventArgs args) => gameData.BreakHistoryGesture();

    private void onWindowActivated(object? sender, EventArgs args)
    {
        gameData.BreakHistoryGesture();
        if (owner.FocusManager?.GetFocusedElement() is Control control && ReferenceEquals(TopLevel.GetTopLevel(control), owner))
            attachFocusedInput(control);
    }

    private void onGotFocus(object? sender, FocusChangedEventArgs args)
    {
        if (args.Source is not Control control)
            return;
        attachFocusedInput(control);
    }

    private void attachFocusedInput(Control control)
    {
        NumericUpDown? numeric = control as NumericUpDown ?? control.FindAncestorOfType<NumericUpDown>();
        if (numeric is not null)
            HistoryMergeBehavior.AttachFocused(numeric, gameData);
        else if (control is TextBox text)
            HistoryMergeBehavior.AttachFocused(text, gameData);
    }

    private void onClosed(object? sender, EventArgs args) => Dispose();
}
