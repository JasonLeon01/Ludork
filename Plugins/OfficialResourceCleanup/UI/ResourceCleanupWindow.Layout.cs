using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Styling;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;

namespace Ludork.Plugins.OfficialResourceCleanup.UI;

internal sealed partial class ResourceCleanupWindow
{
    private readonly TextBox keepInput = EditorInputs.CreateEditableTextBox();
    private readonly ListBox dataList = createList();
    private readonly ListBox assetList = createList();
    private readonly ListBox issueList = createList();
    private readonly ListBox outcomeList = createList();
    private readonly TabControl resultTabs = new();
    private readonly TabItem dataTab = new() { Header = "Data" };
    private readonly TabItem assetTab = new() { Header = "Assets" };
    private readonly TabItem issueTab = new() { IsVisible = false };
    private readonly TabItem outcomeTab = new() { IsVisible = false };
    private readonly Button saveKeepButton = new();
    private readonly Button scanButton = new();
    private readonly Button keepSelectedButton = new();
    private readonly Button trashButton = new();
    private readonly Button cancelButton = new();
    private readonly Button closeButton = new();
    private readonly TextBlock statusText = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock progressText = new();
    private readonly TextBlock currentPathText = new() { TextTrimming = TextTrimming.CharacterEllipsis };
    private readonly ProgressBar progressBar = new() { Height = 5 };
    private readonly StackPanel progressPanel = new() { Spacing = 5 };

    private Control createLayout()
    {
        Grid layout = new()
        {
            Margin = new Thickness(20),
            RowDefinitions = new RowDefinitions("Auto,230,Auto,*,Auto,Auto"),
            RowSpacing = 12,
        };
        TextBlock projectPath = new()
        {
            Text = host.ProjectPath,
            Foreground = PluginTheme.Brush("TextMuted"),
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        ToolTip.SetTip(projectPath, host.ProjectPath);
        StackPanel heading = new() { Spacing = 5 };
        heading.Children.Add(projectPath);
        heading.Children.Add(new TextBlock
        {
            Text = localizer.Text("staticWarning"),
            Foreground = PluginTheme.Brush("Warning"),
            TextWrapping = TextWrapping.Wrap,
        });
        layout.Children.Add(heading);

        Grid keepPanels = new() { ColumnDefinitions = new ColumnDefinitions("*,*"), ColumnSpacing = 12 };
        keepPanels.Children.Add(createNativePanel());
        Control userPanel = createUserPanel();
        Grid.SetColumn(userPanel, 1);
        keepPanels.Children.Add(userPanel);
        Grid.SetRow(keepPanels, 1);
        layout.Children.Add(keepPanels);

        TextBlock scope = new()
        {
            Text = localizer.Text("scope"),
            Foreground = PluginTheme.Brush("TextMuted"),
            TextWrapping = TextWrapping.Wrap,
        };
        Grid.SetRow(scope, 2);
        layout.Children.Add(scope);

        dataList.ItemTemplate = new FuncDataTemplate<ResourceCleanupCandidate>((item, _) => item is null ? null : createCandidateRow(item));
        assetList.ItemTemplate = dataList.ItemTemplate;
        dataList.SelectionMode = SelectionMode.Multiple | SelectionMode.Toggle;
        assetList.SelectionMode = dataList.SelectionMode;
        issueList.ItemTemplate = new FuncDataTemplate<ResourceCleanupIssue>((item, _) => item is null ? null : createIssueRow(item));
        outcomeList.ItemTemplate = issueList.ItemTemplate;
        dataTab.Content = dataList;
        assetTab.Content = assetList;
        issueTab.Content = issueList;
        outcomeTab.Content = outcomeList;
        outcomeTab.Header = localizer.Text("outcome");
        resultTabs.Items.Add(dataTab);
        resultTabs.Items.Add(assetTab);
        resultTabs.Items.Add(issueTab);
        resultTabs.Items.Add(outcomeTab);
        Grid.SetRow(resultTabs, 3);
        layout.Children.Add(resultTabs);

        progressPanel.Children.Add(progressText);
        progressPanel.Children.Add(progressBar);
        progressPanel.Children.Add(currentPathText);
        Grid.SetRow(progressPanel, 4);
        layout.Children.Add(progressPanel);

        StackPanel footer = new() { Spacing = 10 };
        footer.Children.Add(new ScrollViewer
        {
            Content = statusText,
            MaxHeight = 85,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
        });
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(keepSelectedButton);
        buttons.Children.Add(scanButton);
        buttons.Children.Add(trashButton);
        buttons.Children.Add(cancelButton);
        buttons.Children.Add(closeButton);
        footer.Children.Add(buttons);
        Grid.SetRow(footer, 5);
        layout.Children.Add(footer);
        return layout;
    }

    private Control createNativePanel()
    {
        ListBox nativeList = createList();
        nativeList.ItemsSource = nativeKeepPaths;
        nativeList.ItemTemplate = new FuncDataTemplate<string>((path, _) => new TextBlock
        {
            Text = path,
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap,
        });
        Grid panel = new() { RowDefinitions = new RowDefinitions("Auto,*"), RowSpacing = 8 };
        panel.Children.Add(new TextBlock
        {
            Text = localizer.Format("nativeTitle", nativeKeepPaths.Count),
            FontWeight = FontWeight.SemiBold,
        });
        Grid.SetRow(nativeList, 1);
        panel.Children.Add(nativeList);
        return panel;
    }

    private Control createUserPanel()
    {
        Grid panel = new() { RowDefinitions = new RowDefinitions("Auto,Auto,*,Auto"), RowSpacing = 7 };
        panel.Children.Add(new TextBlock { Text = localizer.Text("userTitle"), FontWeight = FontWeight.SemiBold });
        TextBlock description = new()
        {
            Text = localizer.Text("keepHelp"),
            TextWrapping = TextWrapping.Wrap,
            FontSize = 12,
            Foreground = PluginTheme.Brush("TextMuted"),
        };
        Grid.SetRow(description, 1);
        panel.Children.Add(description);
        ScrollViewer.SetHorizontalScrollBarVisibility(keepInput, ScrollBarVisibility.Auto);
        ScrollViewer.SetVerticalScrollBarVisibility(keepInput, ScrollBarVisibility.Auto);
        Grid.SetRow(keepInput, 2);
        panel.Children.Add(keepInput);
        saveKeepButton.HorizontalAlignment = HorizontalAlignment.Right;
        Grid.SetRow(saveKeepButton, 3);
        panel.Children.Add(saveKeepButton);
        return panel;
    }

    private Control createCandidateRow(ResourceCleanupCandidate item)
    {
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,100"), ColumnSpacing = 10, Margin = new Thickness(3, 5) };
        row.Children.Add(new TextBlock { Text = item.RelativePath, TextWrapping = TextWrapping.Wrap });
        TextBlock size = new()
        {
            Text = localizer.Size(item.SizeBytes),
            HorizontalAlignment = HorizontalAlignment.Right,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetColumn(size, 1);
        row.Children.Add(size);
        return row;
    }

    private static Control createIssueRow(ResourceCleanupIssue item)
    {
        StackPanel row = new() { Spacing = 4, Margin = new Thickness(3, 5) };
        row.Children.Add(new TextBlock { Text = item.Path, TextWrapping = TextWrapping.Wrap });
        row.Children.Add(new TextBlock { Text = item.Message, TextWrapping = TextWrapping.Wrap, FontSize = 12 });
        return row;
    }

    private static ListBox createList()
    {
        ListBox list = new()
        {
            ItemsPanel = new FuncTemplate<Panel?>(() => new VirtualizingStackPanel()),
        };
        ScrollViewer.SetHorizontalScrollBarVisibility(list, ScrollBarVisibility.Disabled);
        list.Styles.Add(new Style(selector => selector.OfType<ListBoxItem>())
        {
            Setters = { new Setter(ContentControl.HorizontalContentAlignmentProperty, HorizontalAlignment.Stretch) },
        });
        return list;
    }
}
