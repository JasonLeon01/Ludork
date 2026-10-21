using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintNodePickerWindow : Window
{
    private readonly BlueprintNodePickerCandidates candidates;
    private readonly TextBox searchBox;
    private readonly ListBox itemList;
    private readonly CheckBox? contextSensitiveBox;
    private IReadOnlyList<BlueprintNodePickerRow> rows = [];
    private readonly HashSet<string> expandedGroups = new(StringComparer.Ordinal);
    private readonly DispatcherTimer deactivateTimer;
    private TaskCompletionSource<BlueprintNodePickerSelection?>? completion;
    private BlueprintNodePickerSelection? result;
    private CancellationTokenRegistration cancellationRegistration;
    private CancellationToken cancellationToken;
    private bool canCloseOnDeactivate;

    private BlueprintNodePickerWindow(
        BlueprintNodePickerCandidates candidates,
        PixelPoint position,
        bool contextSensitive)
    {
        this.candidates = candidates;
        Position = position;
        Width = 320;
        Height = 420;
        MinWidth = 260;
        MinHeight = 280;
        WindowStartupLocation = WindowStartupLocation.Manual;
        WindowDecorations = Avalonia.Controls.WindowDecorations.None;
        CanResize = true;
        ShowInTaskbar = false;
        Topmost = true;
        Background = Ludork.Services.EditorTheme.Brush("Surface");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;

        searchBox = EditorInputs.CreateEditableTextBox();
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.TextChanged += (_, _) => rebuildRows();

        itemList = new ListBox
        {
            ItemsSource = rows,
            Background = Ludork.Services.EditorTheme.Brush("Surface"),
            BorderBrush = Ludork.Services.EditorTheme.Brush("Border"),
            BorderThickness = new Thickness(1),
            ItemTemplate = new FuncDataTemplate<BlueprintNodePickerRow>(createRow),
            ItemsPanel = new FuncTemplate<Panel?>(() => new VirtualizingStackPanel()),
        };
        itemList.DoubleTapped += onItemDoubleTapped;

        Grid content = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions(contextSensitive ? "Auto,10,*,10,Auto" : "Auto,10,*"),
        };
        Grid.SetRow(searchBox, 0);
        content.Children.Add(searchBox);
        Grid.SetRow(itemList, 2);
        content.Children.Add(itemList);
        if (contextSensitive)
        {
            contextSensitiveBox = new CheckBox
            {
                Content = LocaleService.Get("CONTEXT_SENSITIVE"),
                IsChecked = true,
            };
            contextSensitiveBox.IsCheckedChanged += (_, _) => rebuildRows();
            Grid.SetRow(contextSensitiveBox, 4);
            content.Children.Add(contextSensitiveBox);
        }
        Content = content;

        deactivateTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(150),
        };
        deactivateTimer.Tick += onDeactivateTimer;
        Opened += onOpened;
        Deactivated += onDeactivated;
        KeyDown += onKeyDown;
        rebuildRows();
    }

    public static Task<BlueprintNodePickerSelection?> ShowAsync(
        Window owner,
        BlueprintNodePickerCandidates candidates,
        PixelPoint position,
        bool contextSensitive = true,
        CancellationToken cancellationToken = default)
    {
        if (cancellationToken.IsCancellationRequested)
            return Task.FromResult<BlueprintNodePickerSelection?>(null);
        BlueprintNodePickerWindow window = new(candidates, position, contextSensitive);
        return window.showAsync(owner, cancellationToken);
    }

    private Task<BlueprintNodePickerSelection?> showAsync(Window owner, CancellationToken cancellationToken)
    {
        if (cancellationToken.IsCancellationRequested)
            return Task.FromResult<BlueprintNodePickerSelection?>(null);
        this.cancellationToken = cancellationToken;
        TaskCompletionSource<BlueprintNodePickerSelection?> source = new(
            TaskCreationOptions.RunContinuationsAsynchronously);
        completion = source;
        Closed += onClosed;
        Show(owner);
        cancellationRegistration = cancellationToken.Register(() => Dispatcher.UIThread.Post(() =>
        {
            if (source.Task.IsCompleted)
                return;
            result = null;
            Close();
        }));
        if (source.Task.IsCompleted)
            cancellationRegistration.Dispose();
        return source.Task;
    }

    private Control? createRow(BlueprintNodePickerRow? row, INameScope? scope)
    {
        if (row is null)
            return null;
        Grid content = new()
        {
            ColumnDefinitions = new ColumnDefinitions("24,*"),
            Margin = new Thickness(row.Depth * 16, 2, 4, 2),
            MinHeight = 28,
        };
        if (row.IsGroup)
        {
            Button toggle = new()
            {
                Content = row.IsExpanded ? "▼" : "▶",
                Width = 22,
                Height = 22,
                Padding = new Thickness(0),
                Background = Brushes.Transparent,
                BorderThickness = new Thickness(0),
                Focusable = false,
            };
            toggle.Click += (_, _) => toggleGroup(row);
            content.Children.Add(toggle);
        }
        TextBlock title = new()
        {
            Text = row.DisplayText,
            TextTrimming = TextTrimming.CharacterEllipsis,
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = EditorTheme.Brush(row.IsGroup ? "Text" : "TextMuted"),
        };
        Grid.SetColumn(title, 1);
        content.Children.Add(title);
        return content;
    }

    private void rebuildRows()
    {
        string search = searchBox.Text?.Trim() ?? string.Empty;
        rows = candidates.GetRows(search, contextSensitiveBox?.IsChecked == true, expandedGroups);
        itemList.ItemsSource = rows;
        itemList.SelectedIndex = search.Length != 0 && rows.Count != 0 ? 0 : -1;
    }

    private void toggleGroup(BlueprintNodePickerRow row)
    {
        if (!row.IsGroup || row.GroupPath is null)
            return;
        if (!expandedGroups.Add(row.GroupPath))
            expandedGroups.Remove(row.GroupPath);
        rebuildRows();
    }

    private void activateSelection()
    {
        if (itemList.SelectedItem is not BlueprintNodePickerRow row)
            return;
        if (row.IsGroup)
        {
            toggleGroup(row);
            return;
        }
        if (row.Selection is null)
            return;
        result = row.Selection;
        Close();
    }

    private void onOpened(object? sender, EventArgs args)
    {
        Activate();
        searchBox.Focus();
        canCloseOnDeactivate = false;
        deactivateTimer.Start();
    }

    private void onDeactivateTimer(object? sender, EventArgs args)
    {
        deactivateTimer.Stop();
        canCloseOnDeactivate = true;
        closeIfInactive();
    }

    private void onDeactivated(object? sender, EventArgs args)
    {
        if (canCloseOnDeactivate)
            Dispatcher.UIThread.Post(closeIfInactive);
    }

    private void closeIfInactive()
    {
        if (IsVisible && !IsActive)
            Close();
    }

    private void onItemDoubleTapped(object? sender, TappedEventArgs args)
    {
        activateSelection();
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Enter)
        {
            activateSelection();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Down && searchBox.IsFocused && rows.Count != 0)
        {
            itemList.SelectedIndex = Math.Max(0, itemList.SelectedIndex);
            itemList.Focus();
            args.Handled = true;
        }
    }

    private void onClosed(object? sender, EventArgs args)
    {
        deactivateTimer.Stop();
        cancellationRegistration.Dispose();
        completion?.TrySetResult(cancellationToken.IsCancellationRequested ? null : result);
    }
}
