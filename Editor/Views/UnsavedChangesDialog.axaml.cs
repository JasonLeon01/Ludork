using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Views;

public enum UnsavedChangesResult
{
    Cancel,
    Save,
    Discard,
}

public partial class UnsavedChangesDialog : Window
{
    public UnsavedChangesDialog() : this([], string.Empty)
    {
    }

    public UnsavedChangesDialog(IReadOnlyList<string> paths, string projectPath)
    {
        InitializeComponent();
        DocumentList.ItemsSource = paths.Select(path => "* " + Path.GetRelativePath(projectPath, path)).OrderBy(path => path).ToArray();
        Title = LocaleService.Get("EXIT");
        MessageText.Text = LocaleService.Get("CONFIRM_EXIT_WITH_UNSAVED_CHANGES");
        SaveButton.Content = LocaleService.Get("SAVE_AND_EXIT");
        DiscardButton.Content = LocaleService.Get("DISCARD_AND_EXIT");
        CancelButton.Content = LocaleService.Get("CANCEL");
    }

    private void onSave(object? sender, RoutedEventArgs args)
    {
        Close(UnsavedChangesResult.Save);
    }

    private void onDiscard(object? sender, RoutedEventArgs args)
    {
        Close(UnsavedChangesResult.Discard);
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close(UnsavedChangesResult.Cancel);
    }
}
