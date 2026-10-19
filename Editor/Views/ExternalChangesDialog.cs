using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class ExternalChangesDialog : Window
{
    private readonly IReadOnlyList<ExternalFileChange> changes;
    private readonly string projectPath;
    private readonly HashSet<string> keepMemory = new(StringComparer.Ordinal);
    private readonly TextBlock counter = new() { Opacity = 0.7 };
    private readonly TextBlock path = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock changeKind = new() { Opacity = 0.7 };
    private readonly CheckBox applyRemaining = new() { Content = LocaleService.Get("EXTERNAL_APPLY_REMAINING") };
    private int index;

    private ExternalChangesDialog(IReadOnlyList<ExternalFileChange> changes, string projectPath)
    {
        this.changes = changes.OrderBy(change => change.Path, StringComparer.Ordinal).ToArray();
        this.projectPath = projectPath;
        Title = LocaleService.Get("EXTERNAL_CHANGES");
        Width = 600;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        Button useDisk = new() { Content = LocaleService.Get("EXTERNAL_USE_DISK") };
        useDisk.Click += (_, _) => choose(false);
        Button retainMemory = new() { Content = LocaleService.Get("EXTERNAL_KEEP_MEMORY"), IsDefault = true };
        retainMemory.Click += (_, _) => choose(true);
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close();
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { useDisk, retainMemory, cancel },
        };
        Content = new StackPanel
        {
            Margin = new Thickness(22),
            Spacing = 18,
            Children =
            {
                new TextBlock { Text = LocaleService.Get("EXTERNAL_CHANGES_DESCRIPTION"), TextWrapping = TextWrapping.Wrap },
                new StackPanel
                {
                    Spacing = 6,
                    Children = { counter, new ScrollViewer { Content = path, MaxHeight = 120 }, changeKind },
                },
                applyRemaining,
                buttons,
            },
        };
        refreshCurrent();
        KeyDown += (_, args) =>
        {
            if (args.Key != Key.Escape)
                return;
            Close();
            args.Handled = true;
        };
    }

    public static Task<IReadOnlySet<string>?> ShowAsync(Window owner, IReadOnlyList<ExternalFileChange> changes, string projectPath)
    {
        if (changes.Count == 0)
            return Task.FromResult<IReadOnlySet<string>?>(new HashSet<string>(StringComparer.Ordinal));
        return new ExternalChangesDialog(changes, projectPath).ShowDialog<IReadOnlySet<string>?>(owner);
    }

    private void choose(bool retainMemory)
    {
        int end = applyRemaining.IsChecked == true ? changes.Count : index + 1;
        while (index < end)
        {
            if (retainMemory)
                keepMemory.Add(changes[index].Path);
            index++;
        }
        if (index == changes.Count)
            Close(keepMemory);
        else
            refreshCurrent();
    }

    private void refreshCurrent()
    {
        ExternalFileChange change = changes[index];
        counter.Text = LocaleService.Get("EXTERNAL_CONFLICT_COUNTER")
            .Replace("{current}", (index + 1).ToString(), StringComparison.Ordinal)
            .Replace("{total}", changes.Count.ToString(), StringComparison.Ordinal);
        path.Text = Path.GetRelativePath(projectPath, change.Path);
        changeKind.Text = LocaleService.Get(change.Fingerprint is null ? "EXTERNAL_DELETED"
            : change.PreviousFingerprint is null ? "EXTERNAL_ADDED" : "EXTERNAL_MODIFIED");
    }
}
