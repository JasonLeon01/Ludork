using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class GeneralDataEditorWindow : Window
{
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintPreviewService previewService;
    private readonly Toast toast;
    private readonly TabControl tabControl;
    private readonly DeferredWindowInitializer initializer;
    private readonly Dictionary<string, BlueprintEditorWindow> blueprintWindows = new(StringComparer.Ordinal);
    private readonly Dictionary<string, GeneralDataPageSessionState> pageStates = new(StringComparer.Ordinal);
    private string? pendingTypeKey;
    private bool buildingTabs;

    public GeneralDataEditorWindow(
        GameDataService gameData,
        ProjectSaveService projectSave,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintPreviewService previewService)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.previewService = previewService;
        Title = LocaleService.Get("GENERAL_DATA_EDITOR");
        Width = 1000;
        Height = 600;
        MinWidth = 700;
        MinHeight = 400;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.FromRgb(30, 30, 30));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        tabControl = new TabControl
        {
            Background = new SolidColorBrush(Color.FromRgb(30, 30, 30)),
        };
        tabControl.AddHandler(PointerPressedEvent, onTabPointerPressed, RoutingStrategies.Bubble);
        tabControl.SelectionChanged += (_, _) =>
        {
            if (!buildingTabs)
                ensureSelectedPage();
        };

        Content = DeferredWindowInitializer.CreateLoadingContent();
        HistoryMergeBehavior.AttachBoundary(this, gameData);
        toast = new Toast(this);
        initializer = new DeferredWindowInitializer(this, () =>
        {
            Content = tabControl;
            buildTabs(pendingTypeKey);
            pendingTypeKey = null;
        });

        gameData.DataRestored += onDataRestored;
        gameData.DataReloaded += onDataReloaded;
        Closed += (_, _) =>
        {
            gameData.DataRestored -= onDataRestored;
            gameData.DataReloaded -= onDataReloaded;
        };
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public void selectDataType(string key)
    {
        if (!initializer.IsInitialized)
        {
            pendingTypeKey = key;
            return;
        }
        selectDataTypeCore(key);
    }

    private void selectDataTypeCore(string key)
    {
        foreach (TabItem tab in tabControl.Items.OfType<TabItem>())
        {
            if (tab.Tag is string tabKey && string.Equals(tabKey, key, StringComparison.Ordinal))
            {
                tabControl.SelectedItem = tab;
                return;
            }
        }
    }

    public void refresh()
    {
        if (!initializer.IsInitialized)
            return;
        string? selectedKey = (tabControl.SelectedItem as TabItem)?.Tag as string;
        buildTabs(selectedKey);
    }

    private void buildTabs(string? preserveKey)
    {
        buildingTabs = true;
        tabControl.Items.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in gameData.GeneralData.OrderBy(e => e.Key, StringComparer.Ordinal))
        {
            TabItem tab = new()
            {
                Header = entry.Key,
                Tag = entry.Key,
            };
            tabControl.Items.Add(tab);
            if (string.Equals(entry.Key, preserveKey, StringComparison.Ordinal))
                tabControl.SelectedItem = tab;
        }
        if (tabControl.SelectedItem is null && tabControl.Items.Count > 0)
            tabControl.SelectedItem = tabControl.Items[0];
        buildingTabs = false;
        ensureSelectedPage();
        foreach (string staleKey in pageStates.Keys.Except(gameData.GeneralData.Keys, StringComparer.Ordinal).ToArray())
            pageStates.Remove(staleKey);
    }

    private void ensureSelectedPage()
    {
        if (tabControl.SelectedItem is not TabItem { Tag: string typeKey } tab
            || tab.Content is not null
            || !gameData.GeneralData.TryGetValue(typeKey, out JsonObject? data))
        {
            return;
        }
        tab.Content = new GeneralDataPage(
            this,
            gameData,
            typeKey,
            data,
            getPageState(typeKey));
    }

    private GeneralDataPageSessionState getPageState(string typeKey)
    {
        if (!pageStates.TryGetValue(typeKey, out GeneralDataPageSessionState? state))
        {
            state = new GeneralDataPageSessionState();
            pageStates[typeKey] = state;
        }
        return state;
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        if (!initializer.IsInitialized)
            return;
        string? selectedKey = (tabControl.SelectedItem as TabItem)?.Tag as string;
        buildTabs(selectedKey);
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        if (!initializer.IsInitialized)
            return;
        string? selectedKey = (tabControl.SelectedItem as TabItem)?.Tag as string;
        buildTabs(selectedKey);
    }

    internal void FlushBlueprintEditors()
    {
        foreach (BlueprintEditorWindow window in blueprintWindows.Values.ToArray())
            window.FlushPendingChanges();
    }

    internal void closeBlueprintEditor(string typeKey, string memberId)
    {
        string key = BlueprintEditorDocument.GetGeneralDocumentKey(typeKey, memberId);
        if (blueprintWindows.TryGetValue(key, out BlueprintEditorWindow? window))
            window.Close();
    }

    private void closeBlueprintEditors(string typeKey)
    {
        string prefix = BlueprintEditorDocument.GetGeneralDocumentPrefix(typeKey);
        foreach (KeyValuePair<string, BlueprintEditorWindow> entry in blueprintWindows
            .Where(entry => entry.Key.StartsWith(prefix, StringComparison.Ordinal))
            .ToArray())
        {
            entry.Value.Close();
        }
    }

    internal void showBlueprintEditor(string typeKey, string memberId)
    {
        BlueprintEditorDocument? document = BlueprintEditorDocument.CreateGeneralData(
            gameData,
            typeKey,
            memberId);
        if (document is null)
            return;
        if (blueprintWindows.TryGetValue(document.DocumentKey, out BlueprintEditorWindow? existing))
        {
            if (!existing.Reload())
                return;
            existing.Show();
            existing.Activate();
            return;
        }
        BlueprintEditorWindow window = new(
            document,
            gameData,
            projectSave,
            metadataService,
            classResolver,
            previewService);
        blueprintWindows[document.DocumentKey] = window;
        window.Closed += (_, _) => blueprintWindows.Remove(document.DocumentKey);
        window.Show(this);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
        {
            FlushBlueprintEditors();
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        }
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", gameData.Redo());
        else
            return;
        args.Handled = true;
    }

    private void onTabPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!args.GetCurrentPoint(this).Properties.IsRightButtonPressed)
            return;
        string? hitKey = null;
        if (args.Source is Visual source)
        {
            Visual? current = source;
            while (current is not null)
            {
                if (current is TabItem item && item.Tag is string key)
                {
                    hitKey = key;
                    break;
                }
                current = current.GetVisualParent();
            }
        }
        args.Handled = true;
        showTabContextMenu(hitKey, args.GetPosition(this));
    }

    private void showTabContextMenu(string? typeKey, Point screenPoint)
    {
        ContextMenu menu = new();
        MenuItem newItem = new() { Header = LocaleService.Get("NEW_DATA_TYPE") };
        newItem.Click += async (_, _) => await onAddDataTypeAsync();
        menu.Items.Add(newItem);
        if (typeKey is not null)
        {
            menu.Items.Add(new Separator());
            MenuItem renameItem = new() { Header = LocaleService.Get("RENAME_DATA_TYPE") };
            renameItem.Click += async (_, _) => await onRenameDataTypeAsync(typeKey);
            menu.Items.Add(renameItem);
            MenuItem addEventItem = new() { Header = LocaleService.Get("NEW_EVENT") };
            addEventItem.Click += async (_, _) => await onAddEventAsync(typeKey);
            menu.Items.Add(addEventItem);
            if (getEventNames(gameData.GeneralData[typeKey]).Count != 0)
            {
                MenuItem renameEventItem = new() { Header = LocaleService.Get("RENAME_EVENT") };
                renameEventItem.Click += async (_, _) => await onRenameEventAsync(typeKey);
                menu.Items.Add(renameEventItem);
                MenuItem deleteEventItem = new() { Header = LocaleService.Get("DELETE_EVENT") };
                deleteEventItem.Click += async (_, _) => await onDeleteEventAsync(typeKey);
                menu.Items.Add(deleteEventItem);
            }
            menu.Items.Add(new Separator());
            MenuItem deleteItem = new() { Header = LocaleService.Get("DELETE_DATA_TYPE") };
            deleteItem.Click += async (_, _) => await onDeleteDataTypeAsync(typeKey);
            menu.Items.Add(deleteItem);
        }
        menu.Open(this);
    }

    private async Task onAddDataTypeAsync()
    {
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_DATA_TYPE"),
            LocaleService.Get("ENTER_DATA_TYPE_NAME"),
            gameData.GeneralData.Keys);
        if (string.IsNullOrWhiteSpace(name))
            return;
        gameData.CreateGeneralType(name);
        buildTabs(name);
    }

    private async Task onRenameDataTypeAsync(string typeKey)
    {
        string? newName = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_DATA_TYPE"),
            LocaleService.Get("ENTER_DATA_TYPE_NAME"),
            gameData.GeneralData.Keys.Where(k => k != typeKey),
            typeKey);
        if (string.IsNullOrWhiteSpace(newName) || newName == typeKey)
            return;
        closeBlueprintEditors(typeKey);
        gameData.RenameGeneralType(typeKey, newName);
        if (pageStates.Remove(typeKey, out GeneralDataPageSessionState? state))
            pageStates[newName] = state;
        buildTabs(newName);
    }

    private async Task onAddEventAsync(string typeKey)
    {
        JsonObject typeData = gameData.GeneralData[typeKey];
        IReadOnlyList<string> eventNames = getEventNames(typeData);
        string? eventName = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            eventNames);
        if (!isValidEventName(eventName))
            return;
        closeBlueprintEditors(typeKey);
        gameData.RecordSnapshot();
        JsonArray events = typeData["events"] as JsonArray ?? new JsonArray();
        events.Add(eventName!.Trim());
        typeData["events"] = events;
        gameData.refreshModifiedState();
        buildTabs(typeKey);
    }

    private async Task onRenameEventAsync(string typeKey)
    {
        JsonObject typeData = gameData.GeneralData[typeKey];
        IReadOnlyList<string> eventNames = getEventNames(typeData);
        string? currentName = await ItemSelectorDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            eventNames);
        if (currentName is null)
            return;
        string? newName = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            eventNames.Where(name => name != currentName),
            currentName);
        if (!isValidEventName(newName) || string.Equals(currentName, newName!.Trim(), StringComparison.Ordinal))
            return;
        closeBlueprintEditors(typeKey);
        gameData.RecordSnapshot();
        JsonArray events = (JsonArray)typeData["events"]!;
        for (int index = 0; index < events.Count; index++)
        {
            if (string.Equals(events[index]?.GetValue<string>(), currentName, StringComparison.Ordinal))
                events[index] = newName.Trim();
        }
        foreachGeneralMember(typeData, member => renameGraphEvent(member, currentName, newName.Trim()));
        gameData.refreshModifiedState();
        buildTabs(typeKey);
    }

    private async Task onDeleteEventAsync(string typeKey)
    {
        JsonObject typeData = gameData.GeneralData[typeKey];
        IReadOnlyList<string> eventNames = getEventNames(typeData);
        string? eventName = await ItemSelectorDialog.ShowAsync(
            this,
            LocaleService.Get("DELETE_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            eventNames);
        if (eventName is null)
            return;
        string message = LocaleService.Get("CONFIRM_DELETE_EVENT")
            .Replace("{name}", eventName, StringComparison.Ordinal);
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            LocaleService.Get("DELETE_EVENT"),
            message);
        if (!confirmed)
            return;
        closeBlueprintEditors(typeKey);
        gameData.RecordSnapshot();
        JsonArray events = (JsonArray)typeData["events"]!;
        for (int index = events.Count - 1; index >= 0; index--)
        {
            if (string.Equals(events[index]?.GetValue<string>(), eventName, StringComparison.Ordinal))
                events.RemoveAt(index);
        }
        if (events.Count == 0)
            typeData.Remove("events");
        foreachGeneralMember(typeData, member => removeGraphEvent(member, eventName));
        gameData.refreshModifiedState();
        buildTabs(typeKey);
    }

    private async Task onDeleteDataTypeAsync(string typeKey)
    {
        string msg = LocaleService.Get("CONFIRM_DELETE_DATA_TYPE").Replace("{}", typeKey);
        bool confirmed = await ConfirmationDialog.ShowAsync(this, LocaleService.Get("DELETE_DATA_TYPE"), msg);
        if (!confirmed)
            return;
        closeBlueprintEditors(typeKey);
        gameData.DeleteGeneralType(typeKey);
        pageStates.Remove(typeKey);
        buildTabs(null);
    }

    private static IReadOnlyList<string> getEventNames(JsonObject typeData)
    {
        return typeData["events"] is JsonArray events
            ? events.Select(value => value?.GetValue<string>() ?? string.Empty).ToArray()
            : [];
    }

    private static bool isValidEventName(string? eventName)
    {
        string value = eventName?.Trim() ?? string.Empty;
        return value.Length != 0 && !char.IsDigit(value[0]);
    }

    private static void foreachGeneralMember(JsonObject typeData, Action<JsonObject> action)
    {
        if (typeData["members"] is not JsonObject members)
            return;
        foreach (JsonObject member in members.Select(entry => entry.Value).OfType<JsonObject>())
            action(member);
    }

    private static void renameGraphEvent(JsonObject member, string oldName, string newName)
    {
        if (member["_graph"] is not JsonObject graph)
            return;
        if (graph["nodeGraph"] is JsonObject nodeGraph && nodeGraph.ContainsKey(oldName))
            graph["nodeGraph"] = renameObjectKey(nodeGraph, oldName, newName);
        if (graph["startNodes"] is JsonObject startNodes && startNodes.ContainsKey(oldName))
            graph["startNodes"] = renameObjectKey(startNodes, oldName, newName);
    }

    private static void removeGraphEvent(JsonObject member, string eventName)
    {
        if (member["_graph"] is not JsonObject graph)
            return;
        if (graph["nodeGraph"] is JsonObject nodeGraph)
            nodeGraph.Remove(eventName);
        if (graph["startNodes"] is JsonObject startNodes)
            startNodes.Remove(eventName);
    }

    private static JsonObject renameObjectKey(JsonObject source, string oldName, string newName)
    {
        JsonObject result = [];
        foreach (KeyValuePair<string, JsonNode?> entry in source)
            result[entry.Key == oldName ? newName : entry.Key] = entry.Value?.DeepClone();
        return result;
    }
}

internal enum GeneralDataViewMode
{
    Form,
    Table,
}

internal sealed class GeneralDataPageSessionState
{
    public string SearchText { get; set; } = string.Empty;
    public GeneralDataViewMode ViewMode { get; set; }
    public string? SelectedMemberId { get; set; }
}

internal sealed class GeneralDataPage : Grid
{
    private const double TableIdColumnWidth = 180;
    private const double TableFieldColumnWidth = 200;
    private readonly GeneralDataEditorWindow owner;
    private readonly GameDataService gameData;
    private readonly string typeKey;
    private readonly JsonObject typeData;
    private readonly GeneralDataPageSessionState sessionState;
    private readonly TextBox searchBox;
    private readonly ListBox memberList;
    private readonly Border actionBar;
    private readonly Button editAbilityGraphButton;
    private readonly Button formViewButton;
    private readonly Button tableViewButton;
    private readonly ScrollViewer formScroll;
    private readonly StackPanel formContent;
    private readonly ScrollViewer tableScroll;
    private readonly Grid tableSurface;
    private readonly Grid tableHeader;
    private readonly ListBox tableList;
    private List<GeneralDataTableColumn> tableColumns = [];

    private string? selectedMemberId;
    private bool syncingSelection;

    public GeneralDataPage(
        GeneralDataEditorWindow owner,
        GameDataService gameData,
        string typeKey,
        JsonObject typeData,
        GeneralDataPageSessionState sessionState)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.typeKey = typeKey;
        this.typeData = typeData;
        this.sessionState = sessionState;

        ColumnDefinitions = new ColumnDefinitions("240,4,*");

        Grid leftGrid = new() { RowDefinitions = new RowDefinitions("Auto,*,Auto") };
        searchBox = EditorInputs.CreateEditableTextBox(sessionState.SearchText);
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.Margin = new Thickness(4);
        searchBox.TextChanged += (_, _) =>
        {
            sessionState.SearchText = searchBox.Text ?? string.Empty;
            populateMemberList(sessionState.SelectedMemberId);
        };
        Grid.SetRow(searchBox, 0);
        leftGrid.Children.Add(searchBox);

        memberList = new ListBox
        {
            Background = new SolidColorBrush(Color.FromRgb(40, 40, 40)),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        memberList.SelectionChanged += onMemberSelectionChanged;
        memberList.AddHandler(PointerPressedEvent, onMemberListPointerPressed, RoutingStrategies.Bubble);
        Grid.SetRow(memberList, 1);
        leftGrid.Children.Add(memberList);

        Button addMemberBtn = new()
        {
            Content = "+ " + LocaleService.Get("NEW_MEMBER"),
            Margin = new Thickness(4),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        addMemberBtn.Click += async (_, _) => await onAddMemberAsync();
        Grid.SetRow(addMemberBtn, 2);
        leftGrid.Children.Add(addMemberBtn);
        Children.Add(leftGrid);

        GridSplitter splitter = new()
        {
            Width = 4,
            Background = new SolidColorBrush(Color.FromRgb(50, 50, 50)),
            VerticalAlignment = VerticalAlignment.Stretch,
        };
        Grid.SetColumn(splitter, 1);
        Children.Add(splitter);

        Grid rightGrid = new() { RowDefinitions = new RowDefinitions("Auto,*") };
        actionBar = new Border
        {
            Background = new SolidColorBrush(Color.FromRgb(45, 45, 45)),
            Padding = new Thickness(8, 6),
        };
        editAbilityGraphButton = new Button
        {
            Content = LocaleService.Get("EDIT_ABILITY_GRAPH"),
            IsEnabled = false,
        };
        editAbilityGraphButton.Click += (_, _) => openSelectedAbilityGraph();
        formViewButton = new Button
        {
            Content = LocaleService.Get("GENERAL_DATA_FORM_VIEW"),
            MinWidth = 72,
        };
        formViewButton.Click += (_, _) => setViewMode(GeneralDataViewMode.Form);
        tableViewButton = new Button
        {
            Content = LocaleService.Get("GENERAL_DATA_TABLE_VIEW"),
            MinWidth = 72,
        };
        tableViewButton.Click += (_, _) => setViewMode(GeneralDataViewMode.Table);
        Grid actionContent = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto,8,Auto,4,Auto"),
        };
        Grid.SetColumn(editAbilityGraphButton, 1);
        actionContent.Children.Add(editAbilityGraphButton);
        Grid.SetColumn(formViewButton, 3);
        actionContent.Children.Add(formViewButton);
        Grid.SetColumn(tableViewButton, 5);
        actionContent.Children.Add(tableViewButton);
        actionBar.Child = actionContent;
        Grid.SetRow(actionBar, 0);
        rightGrid.Children.Add(actionBar);

        formContent = new StackPanel { Spacing = 6, Margin = new Thickness(8) };
        formScroll = new ScrollViewer
        {
            Content = formContent,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled,
        };
        Grid.SetRow(formScroll, 1);
        rightGrid.Children.Add(formScroll);

        tableHeader = new Grid
        {
            Background = new SolidColorBrush(Color.Parse("#333333")),
        };
        tableList = new ListBox
        {
            Background = new SolidColorBrush(Color.FromRgb(40, 40, 40)),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = new FuncDataTemplate<GeneralDataTableRow>(buildTableRow),
        };
        tableList.SelectionChanged += onTableSelectionChanged;
        tableList.AddHandler(PointerPressedEvent, onTablePointerPressed, RoutingStrategies.Tunnel);
        tableSurface = new Grid
        {
            RowDefinitions = new RowDefinitions("Auto,*"),
            HorizontalAlignment = HorizontalAlignment.Left,
        };
        tableSurface.Children.Add(tableHeader);
        Grid.SetRow(tableList, 1);
        tableSurface.Children.Add(tableList);
        tableScroll = new ScrollViewer
        {
            Content = tableSurface,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled,
            IsVisible = false,
        };
        Grid.SetRow(tableScroll, 1);
        rightGrid.Children.Add(tableScroll);

        Grid.SetColumn(rightGrid, 2);
        Children.Add(rightGrid);

        populateMemberList(sessionState.SelectedMemberId);
        updateActionBar();
        updateViewMode();
    }

    private void populateMemberList(string? preferredMemberId = null)
    {
        List<string> memberIds = [];
        if (typeData["members"] is JsonObject members)
        {
            memberIds = members
                .Where(entry => entry.Value is JsonObject member && matchesSearch(entry.Key, member))
                .Select(entry => entry.Key)
                .OrderBy(id => id, StringComparer.Ordinal)
                .ToList();
        }
        string? nextSelection = preferredMemberId is not null && memberIds.Contains(preferredMemberId)
            ? preferredMemberId
            : sessionState.SelectedMemberId is not null && memberIds.Contains(sessionState.SelectedMemberId)
                ? sessionState.SelectedMemberId
                : memberIds.FirstOrDefault();
        syncingSelection = true;
        memberList.ItemsSource = memberIds;
        memberList.SelectedItem = nextSelection;
        syncingSelection = false;
        selectMember(nextSelection);
        if (sessionState.ViewMode == GeneralDataViewMode.Table)
            rebuildTable();
    }

    private void updateActionBar()
    {
        editAbilityGraphButton.IsVisible = hasEvents();
        updateEditAbilityGraphButton();
    }

    private void onMemberSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!syncingSelection)
            selectMember(memberList.SelectedItem as string);
    }

    private bool matchesSearch(string memberId, JsonObject member)
    {
        string query = sessionState.SearchText.Trim();
        if (query.Length == 0 || memberId.Contains(query, StringComparison.OrdinalIgnoreCase))
            return true;
        foreach (JsonNode? value in member.Select(entry => entry.Value))
        {
            if (value is JsonValue scalar
                && scalar.ToJsonString().Contains(query, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    private void selectMember(string? memberId)
    {
        selectedMemberId = memberId;
        sessionState.SelectedMemberId = memberId;
        syncingSelection = true;
        if (!Equals(memberList.SelectedItem, memberId))
            memberList.SelectedItem = memberId;
        if (tableList.ItemsSource is IEnumerable<GeneralDataTableRow> rows)
        {
            GeneralDataTableRow? row = memberId is null
                ? null
                : rows.FirstOrDefault(item => item.Id == memberId);
            if (!Equals(tableList.SelectedItem, row))
                tableList.SelectedItem = row;
        }
        syncingSelection = false;
        if (sessionState.ViewMode == GeneralDataViewMode.Form)
            buildForm(memberId);
        updateEditAbilityGraphButton();
    }

    private void setViewMode(GeneralDataViewMode mode)
    {
        if (sessionState.ViewMode == mode)
            return;
        sessionState.ViewMode = mode;
        updateViewMode();
    }

    private void updateViewMode()
    {
        bool showForm = sessionState.ViewMode == GeneralDataViewMode.Form;
        formScroll.IsVisible = showForm;
        tableScroll.IsVisible = !showForm;
        formViewButton.IsEnabled = !showForm;
        tableViewButton.IsEnabled = showForm;
        if (showForm)
            buildForm(selectedMemberId);
        else
            rebuildTable();
    }

    private void rebuildTable()
    {
        tableColumns = [];
        if (typeData["params"] is JsonObject paramsObj)
        {
            foreach (KeyValuePair<string, JsonNode?> entry in paramsObj)
            {
                if (entry.Value is JsonObject definition)
                    tableColumns.Add(new GeneralDataTableColumn(entry.Key, definition));
            }
        }
        string definitions = string.Join(
            ",",
            new[] { TableIdColumnWidth }.Concat(Enumerable.Repeat(TableFieldColumnWidth, tableColumns.Count)));
        tableHeader.ColumnDefinitions = new ColumnDefinitions(definitions);
        tableHeader.Children.Clear();
        tableHeader.Children.Add(buildTableHeaderCell("ID", 0));
        for (int index = 0; index < tableColumns.Count; index++)
        {
            GeneralDataTableColumn column = tableColumns[index];
            tableHeader.Children.Add(buildTableHeaderCell(
                column.Name,
                index + 1,
                column.Definition["comment"]?.GetValue<string>()));
        }
        tableSurface.Width = TableIdColumnWidth + tableColumns.Count * TableFieldColumnWidth;

        GeneralDataTableRow[] rows = typeData["members"] is JsonObject members
            ? members
                .Where(entry => entry.Value is JsonObject member && matchesSearch(entry.Key, member))
                .OrderBy(entry => entry.Key, StringComparer.Ordinal)
                .Select(entry => new GeneralDataTableRow(entry.Key, (JsonObject)entry.Value!))
                .ToArray()
            : [];
        syncingSelection = true;
        tableList.ItemsSource = rows;
        tableList.SelectedItem = selectedMemberId is null
            ? null
            : rows.FirstOrDefault(row => row.Id == selectedMemberId);
        syncingSelection = false;
    }

    private static Border buildTableHeaderCell(string text, int column, string? comment = null)
    {
        Border border = new()
        {
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
            BorderThickness = new Thickness(0, 0, 1, 1),
            Padding = new Thickness(8, 6),
            Child = new TextBlock
            {
                Text = text,
                FontWeight = FontWeight.Bold,
                TextTrimming = TextTrimming.CharacterEllipsis,
            },
        };
        if (!string.IsNullOrWhiteSpace(comment))
            ToolTip.SetTip(border, comment);
        Grid.SetColumn(border, column);
        return border;
    }

    private Control? buildTableRow(GeneralDataTableRow? row, INameScope? scope)
    {
        if (row is null)
            return null;
        string definitions = string.Join(
            ",",
            new[] { TableIdColumnWidth }.Concat(Enumerable.Repeat(TableFieldColumnWidth, tableColumns.Count)));
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions(definitions),
            Width = TableIdColumnWidth + tableColumns.Count * TableFieldColumnWidth,
        };
        grid.AddHandler(
            PointerPressedEvent,
            (_, _) => tableList.SelectedItem = row,
            RoutingStrategies.Tunnel);
        grid.Children.Add(buildTableCell(
            new TextBlock
            {
                Text = row.Id,
                VerticalAlignment = VerticalAlignment.Center,
                TextTrimming = TextTrimming.CharacterEllipsis,
            },
            0));
        for (int index = 0; index < tableColumns.Count; index++)
        {
            Control editor = buildTableEditor(row, tableColumns[index]);
            grid.Children.Add(buildTableCell(editor, index + 1));
        }
        return grid;
    }

    private static Border buildTableCell(Control content, int column)
    {
        Border border = new()
        {
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
            BorderThickness = new Thickness(0, 0, 1, 1),
            Padding = new Thickness(6, 4),
            MinHeight = 42,
            Child = content,
        };
        Grid.SetColumn(border, column);
        return border;
    }

    private Control buildTableEditor(GeneralDataTableRow row, GeneralDataTableColumn column)
    {
        string type = column.Definition["type"]?.GetValue<string>() ?? "string";
        JsonNode? rawValue = row.Member[column.Name];
        if (type == "bool")
        {
            CheckBox check = new()
            {
                IsChecked = rawValue?.GetValue<bool?>() ?? false,
                VerticalAlignment = VerticalAlignment.Center,
            };
            check.IsCheckedChanged += (_, _) =>
            {
                bool next = check.IsChecked ?? false;
                if ((row.Member[column.Name]?.GetValue<bool?>() ?? false) == next)
                    return;
                gameData.RecordSnapshot();
                row.Member[column.Name] = next;
                gameData.refreshModifiedState();
            };
            return check;
        }
        if (type == "int")
        {
            NumericUpDown number = EditorInputs.CreateNumericUpDown(
                rawValue?.GetValue<int?>() ?? 0,
                -999999,
                999999,
                1);
            HistoryMergeBehavior.Attach(number, gameData);
            number.ValueChanged += (_, _) =>
            {
                int next = (int)(number.Value ?? 0);
                if ((row.Member[column.Name]?.GetValue<int?>() ?? 0) == next)
                    return;
                gameData.RecordSnapshot();
                row.Member[column.Name] = next;
                gameData.refreshModifiedState();
            };
            return number;
        }
        if (type == "float")
        {
            NumericUpDown number = EditorInputs.CreateNumericUpDown(
                (decimal)(rawValue?.GetValue<double?>() ?? 0.0),
                -999999,
                999999,
                0.01m);
            HistoryMergeBehavior.Attach(number, gameData);
            number.ValueChanged += (_, _) =>
            {
                double next = (double)(number.Value ?? 0);
                if (Math.Abs((row.Member[column.Name]?.GetValue<double?>() ?? 0.0) - next) < 1e-10)
                    return;
                gameData.RecordSnapshot();
                row.Member[column.Name] = next;
                gameData.refreshModifiedState();
            };
            return number;
        }
        if (type == "string")
        {
            string current = rawValue?.GetValue<string>() ?? string.Empty;
            JsonObject? reference = getParamReference(column.Definition);
            string refKind = reference?["kind"]?.GetValue<string>() ?? string.Empty;
            string refKey = reference?["key"]?.GetValue<string>() ?? string.Empty;
            List<string>? options = getRefOptions(refKind, refKey);
            if (options is not null)
            {
                ComboBox combo = GeneralDataReferenceInputs.Create(current, options);
                combo.SelectionChanged += (_, _) =>
                {
                    string next = GeneralDataReferenceInputs.GetValue(combo);
                    if ((row.Member[column.Name]?.GetValue<string>() ?? string.Empty) == next)
                        return;
                    gameData.RecordSnapshot();
                    row.Member[column.Name] = next;
                    gameData.refreshModifiedState();
                };
                return combo;
            }
            TextBox text = EditorInputs.CreateEditableTextBox(current);
            HistoryMergeBehavior.Attach(text, gameData);
            text.TextChanged += (_, _) =>
            {
                string next = text.Text ?? string.Empty;
                if ((row.Member[column.Name]?.GetValue<string>() ?? string.Empty) == next)
                    return;
                gameData.RecordSnapshot();
                row.Member[column.Name] = next;
                gameData.refreshModifiedState();
            };
            return text;
        }
        Grid complex = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 4,
        };
        TextBlock summary = new()
        {
            Text = summarizeComplexValue(type, rawValue),
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        complex.Children.Add(summary);
        Button edit = new()
        {
            Content = LocaleService.Get("EDIT"),
            MinWidth = 54,
        };
        edit.Click += (_, _) => showMemberInForm(row.Id);
        Grid.SetColumn(edit, 1);
        complex.Children.Add(edit);
        return complex;
    }

    private static string summarizeComplexValue(string type, JsonNode? value)
    {
        if (type == "file")
            return value?.GetValue<string>() ?? string.Empty;
        if (value is JsonArray array)
            return $"[{array.Count}]";
        if (value is JsonObject obj)
            return $"{{{obj.Count}}}";
        return value?.ToJsonString() ?? "—";
    }

    private void showMemberInForm(string memberId)
    {
        if (typeData["members"]?[memberId] is JsonObject member && !matchesSearch(memberId, member))
        {
            sessionState.SearchText = string.Empty;
            searchBox.Text = string.Empty;
        }
        sessionState.SelectedMemberId = memberId;
        sessionState.ViewMode = GeneralDataViewMode.Form;
        populateMemberList(memberId);
        updateViewMode();
    }

    private void revealMember(string memberId)
    {
        if (typeData["members"]?[memberId] is JsonObject member && !matchesSearch(memberId, member))
        {
            sessionState.SearchText = string.Empty;
            searchBox.Text = string.Empty;
        }
        sessionState.SelectedMemberId = memberId;
        populateMemberList(memberId);
    }

    private void onTableSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!syncingSelection && tableList.SelectedItem is GeneralDataTableRow row)
            selectMember(row.Id);
    }

    private void onTablePointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!args.GetCurrentPoint(this).Properties.IsRightButtonPressed)
            return;
        string? hitId = null;
        if (args.Source is Visual source)
        {
            Visual? current = source;
            while (current is not null)
            {
                if (current is ListBoxItem item && item.Content is GeneralDataTableRow row)
                {
                    hitId = row.Id;
                    break;
                }
                current = current.GetVisualParent();
            }
        }
        if (hitId is not null)
            selectMember(hitId);
        args.Handled = true;
        showMemberContextMenu(hitId, tableList);
    }

    private void onMemberListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!args.GetCurrentPoint(this).Properties.IsRightButtonPressed)
            return;
        string? hitId = null;
        if (args.Source is Visual source)
        {
            Visual? current = source;
            while (current is not null)
            {
                if (current is ListBoxItem item && item.Content is string id)
                {
                    hitId = id;
                    break;
                }
                current = current.GetVisualParent();
            }
        }
        args.Handled = true;
        showMemberContextMenu(hitId, memberList);
    }

    private void showMemberContextMenu(string? memberId, Control anchor)
    {
        ContextMenu menu = new();
        if (memberId is not null)
        {
            MenuItem changeIdItem = new() { Header = LocaleService.Get("CHANGE_ID") };
            changeIdItem.Click += async (_, _) => await onChangeMemberIdAsync(memberId);
            menu.Items.Add(changeIdItem);

            MenuItem duplicateItem = new() { Header = LocaleService.Get("DUPLICATE_MEMBER") };
            duplicateItem.Click += async (_, _) => await onDuplicateMemberAsync(memberId);
            menu.Items.Add(duplicateItem);

            if (canEditAbilityGraph(memberId))
            {
                MenuItem editAbilityGraphItem = new() { Header = LocaleService.Get("EDIT_ABILITY_GRAPH") };
                editAbilityGraphItem.Click += (_, _) => owner.showBlueprintEditor(typeKey, memberId);
                menu.Items.Add(editAbilityGraphItem);
            }

            menu.Items.Add(new Separator());

            MenuItem removeItem = new() { Header = LocaleService.Get("REMOVE_MEMBER") };
            removeItem.Click += (_, _) => onRemoveMember(memberId);
            menu.Items.Add(removeItem);
        }
        menu.Open(anchor);
    }

    private void updateEditAbilityGraphButton()
    {
        editAbilityGraphButton.IsEnabled = canEditAbilityGraph(selectedMemberId);
    }

    private bool canEditAbilityGraph(string? memberId)
    {
        if (string.IsNullOrWhiteSpace(memberId)
            || typeData["members"]?[memberId] is not JsonObject
            || typeData["events"] is not JsonArray events)
        {
            return false;
        }
        return events.Any(value => value is JsonValue scalar
            && scalar.TryGetValue(out string? eventName)
            && !string.IsNullOrWhiteSpace(eventName));
    }

    private bool hasEvents()
    {
        return typeData["events"] is JsonArray events
            && events.Any(value => value is JsonValue scalar
                && scalar.TryGetValue(out string? eventName)
                && !string.IsNullOrWhiteSpace(eventName));
    }

    private void openSelectedAbilityGraph()
    {
        if (canEditAbilityGraph(selectedMemberId))
            owner.showBlueprintEditor(typeKey, selectedMemberId!);
    }

    private async Task onAddMemberAsync()
    {
        JsonObject? members = typeData["members"] as JsonObject;
        members ??= new JsonObject();
        string? id = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("NEW_MEMBER"),
            LocaleService.Get("ENTER_ID"),
            members.Select(e => e.Key));
        if (string.IsNullOrWhiteSpace(id))
            return;
        gameData.RecordSnapshot();
        JsonObject newMember = buildDefaultMember();
        members[id] = newMember;
        typeData["members"] = members;
        gameData.refreshModifiedState();
        revealMember(id);
    }

    private async Task onChangeMemberIdAsync(string oldId)
    {
        JsonObject? members = typeData["members"] as JsonObject;
        if (members is null)
            return;
        string? newId = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("CHANGE_ID"),
            LocaleService.Get("ENTER_ID"),
            members.Select(e => e.Key).Where(k => k != oldId),
            oldId);
        if (string.IsNullOrWhiteSpace(newId) || newId == oldId)
            return;
        owner.closeBlueprintEditor(typeKey, oldId);
        gameData.RecordSnapshot();
        reorderMemberKey(members, oldId, newId);
        gameData.refreshModifiedState();
        revealMember(newId);
    }

    private async Task onDuplicateMemberAsync(string sourceId)
    {
        JsonObject? members = typeData["members"] as JsonObject;
        if (members is null || members[sourceId] is not JsonObject source)
            return;
        string newId = sourceId + "_copy";
        int counter = 2;
        while (members.ContainsKey(newId))
            newId = sourceId + "_copy" + counter++;
        string? confirmedId = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("DUPLICATE_MEMBER"),
            LocaleService.Get("ENTER_ID"),
            members.Select(e => e.Key),
            newId);
        if (string.IsNullOrWhiteSpace(confirmedId))
            return;
        gameData.RecordSnapshot();
        members[confirmedId] = (JsonObject)source.DeepClone();
        gameData.refreshModifiedState();
        revealMember(confirmedId);
    }

    private void onRemoveMember(string memberId)
    {
        JsonObject? members = typeData["members"] as JsonObject;
        if (members is null)
            return;
        owner.closeBlueprintEditor(typeKey, memberId);
        gameData.RecordSnapshot();
        members.Remove(memberId);
        gameData.refreshModifiedState();
        populateMemberList();
    }

    private void buildForm(string? memberId)
    {
        formContent.Children.Clear();
        if (memberId is null || typeData["members"] is not JsonObject members || members[memberId] is not JsonObject member)
        {
            TextBlock placeholder = new()
            {
                Text = LocaleService.Get(sessionState.SearchText.Trim().Length == 0
                    ? "GENERAL_DATA_NO_MEMBERS"
                    : "GENERAL_DATA_NO_MATCHES"),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(0, 40),
                Foreground = new SolidColorBrush(Color.FromRgb(130, 130, 130)),
            };
            formContent.Children.Add(placeholder);
            return;
        }

        JsonObject? paramsObj = typeData["params"] as JsonObject;

        TextBox idBox = EditorInputs.CreateReadOnlyTextBox(memberId);
        formContent.Children.Add(buildFormRow("ID", idBox, null, null));

        if (paramsObj is not null)
        {
            foreach (KeyValuePair<string, JsonNode?> paramEntry in paramsObj)
            {
                if (paramEntry.Value is not JsonObject paramDef)
                    continue;
                string paramName = paramEntry.Key;
                JsonNode? rawValue = member[paramName];
                Control editor = buildFieldEditor(paramName, paramDef, rawValue, member);
                Control row = buildFormRow(paramName, editor, paramsObj, paramName);
                formContent.Children.Add(row);
            }
        }

        Button addParamBtn = new()
        {
            Content = "+ " + LocaleService.Get("ADD_PARAM"),
            HorizontalAlignment = HorizontalAlignment.Left,
            Margin = new Thickness(0, 6, 0, 0),
        };
        addParamBtn.Click += async (_, _) => await onAddParamAsync(memberId);
        formContent.Children.Add(addParamBtn);
    }

    private Control buildFormRow(string label, Control editor, JsonObject? paramsObj, string? paramName)
    {
        JsonObject? paramDef = paramName is not null ? paramsObj?[paramName] as JsonObject : null;
        JsonObject? reference = getParamReference(paramDef);
        string referenceLabel = reference is null ? string.Empty : formatReferenceLabel(reference);
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("150,8,*,Auto"),
            Margin = new Thickness(0, 2),
        };
        TextBlock labelBlock = new()
        {
            Text = referenceLabel.Length == 0 ? label : label + " [" + referenceLabel + "]",
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
        };
        string comment = paramDef?["comment"]?.GetValue<string>() ?? string.Empty;
        if (comment.Length > 0 || referenceLabel.Length > 0)
        {
            ToolTip.SetTip(
                labelBlock,
                comment.Length > 0 && referenceLabel.Length > 0
                    ? comment + Environment.NewLine + referenceLabel
                    : comment + referenceLabel);
        }
        if (paramName is not null && paramsObj is not null)
        {
            labelBlock.AddHandler(PointerPressedEvent, (s, e) =>
            {
                if (e.GetCurrentPoint(null).Properties.IsRightButtonPressed)
                {
                    e.Handled = true;
                    showParamLabelContextMenu(paramName, paramsObj, labelBlock);
                }
            }, RoutingStrategies.Tunnel);
            labelBlock.Cursor = new Cursor(StandardCursorType.Hand);
        }
        Grid.SetColumn(labelBlock, 0);
        row.Children.Add(labelBlock);
        Grid.SetColumn(editor, 2);
        row.Children.Add(editor);
        if (paramName is not null && paramsObj is not null)
        {
            Button removeBtn = new()
            {
                Content = "−",
                Width = 28,
                Height = 28,
                Padding = new Thickness(0),
                VerticalAlignment = VerticalAlignment.Center,
                Foreground = new SolidColorBrush(Color.FromRgb(200, 80, 80)),
            };
            removeBtn.Click += async (_, _) => await onRemoveParamAsync(paramName);
            Grid.SetColumn(removeBtn, 3);
            row.Children.Add(removeBtn);
        }
        return row;
    }

    private void showParamLabelContextMenu(string paramName, JsonObject paramsObj, Control anchor)
    {
        JsonObject? paramDef = paramsObj[paramName] as JsonObject;
        if (paramDef is null)
            return;

        ContextMenu menu = new();
        MenuItem editItem = new() { Header = LocaleService.Get("EDIT_PARAM") };
        editItem.Click += async (_, _) => await onEditParamAsync(paramName);
        menu.Items.Add(editItem);

        if (!isParamReferenceAllowed(paramDef))
        {
            menu.Open(anchor);
            return;
        }

        menu.Items.Add(new Separator());
        MenuItem addReferenceItem = new() { Header = LocaleService.Get("ADD_REFERENCE") };
        foreach (string key in gameData.GeneralData.Keys.OrderBy(k => k, StringComparer.Ordinal))
        {
            string targetKey = key;
            MenuItem targetItem = new() { Header = targetKey };
            targetItem.Click += (_, _) => setParamReference(
                paramDef!,
                new JsonObject
                {
                    ["kind"] = "general",
                    ["key"] = targetKey,
                });
            addReferenceItem.Items.Add(targetItem);
        }
        addReferenceItem.Items.Add(new Separator());
        MenuItem animationItem = new() { Header = LocaleService.Get("REFERENCE_TYPE_ANIMATION") };
        animationItem.Click += (_, _) => setParamReference(
            paramDef!,
            new JsonObject { ["kind"] = "animation" });
        addReferenceItem.Items.Add(animationItem);
        menu.Items.Add(addReferenceItem);

        if (getParamReference(paramDef) is not null)
        {
            menu.Items.Add(new Separator());
            MenuItem removeRefItem = new() { Header = LocaleService.Get("REMOVE_REFERENCE") };
            removeRefItem.Click += (_, _) => setParamReference(paramDef!, null);
            menu.Items.Add(removeRefItem);
        }
        menu.Open(anchor);
    }

    private void setParamReference(JsonObject paramDef, JsonObject? reference)
    {
        if (JsonNode.DeepEquals(paramDef["reference"], reference))
            return;
        gameData.RecordSnapshot();
        if (reference is null)
            paramDef.Remove("reference");
        else
            paramDef["reference"] = reference;
        gameData.refreshModifiedState();
        buildForm(selectedMemberId);
    }

    private static bool isParamReferenceAllowed(JsonObject? paramDef)
    {
        if (paramDef is null)
            return false;
        string type = paramDef["type"]?.GetValue<string>() ?? "string";
        return type == "string"
            || type == "dict"
            || (type == "list" && getContainerItemType(paramDef, "itemType") == "string");
    }

    private static JsonObject? getParamReference(JsonObject? paramDef)
    {
        if (!isParamReferenceAllowed(paramDef)
            || paramDef!["reference"] is not JsonObject reference)
        {
            return null;
        }
        string kind = reference["kind"]?.GetValue<string>() ?? string.Empty;
        if (kind == "animation")
            return reference;
        string key = reference["key"]?.GetValue<string>() ?? string.Empty;
        return kind == "general" && key.Length > 0 ? reference : null;
    }

    private static string formatReferenceLabel(JsonObject reference)
    {
        return reference["kind"]?.GetValue<string>() == "animation"
            ? LocaleService.Get("REFERENCE_TYPE_ANIMATION")
            : reference["key"]?.GetValue<string>() ?? string.Empty;
    }

    private async Task onAddParamAsync(string memberId)
    {
        JsonObject? paramsObj = typeData["params"] as JsonObject;
        paramsObj ??= new JsonObject();
        GeneralDataParamCreation? result = await AddParamDialog.ShowAsync(
            owner, paramsObj.Select(e => e.Key));
        if (result is null)
            return;
        JsonObject paramDef = buildParamDefinition(result);
        gameData.RecordSnapshot();
        paramsObj[result.Name] = paramDef;
        typeData["params"] = paramsObj;
        if (typeData["members"] is JsonObject members)
        {
            foreach (KeyValuePair<string, JsonNode?> mEntry in members)
            {
                if (mEntry.Value is JsonObject member && !member.ContainsKey(result.Name))
                    member[result.Name] = buildMemberDefaultValue(paramDef);
            }
        }
        gameData.refreshModifiedState();
        buildForm(memberId);
    }

    private async Task onEditParamAsync(string paramName)
    {
        if (typeData["params"] is not JsonObject paramsObj
            || paramsObj[paramName] is not JsonObject currentDefinition)
        {
            return;
        }

        GeneralDataParamCreation initialValue = createParamCreation(paramName, currentDefinition);
        GeneralDataParamCreation? result = await AddParamDialog.ShowEditAsync(
            owner,
            paramsObj.Select(entry => entry.Key).Where(name => name != paramName),
            initialValue);
        if (result is null)
            return;

        JsonObject nextDefinition = updateParamDefinition(currentDefinition, result);
        bool resetMemberValues = hasValueTypeChanged(initialValue, result);
        if (result.Name == paramName && JsonNode.DeepEquals(currentDefinition, nextDefinition))
            return;
        if (resetMemberValues)
        {
            string message = LocaleService.Get("CONFIRM_CHANGE_PARAM_TYPE").Replace("{}", paramName);
            bool confirmed = await ConfirmationDialog.ShowAsync(
                owner,
                LocaleService.Get("EDIT_PARAM"),
                message);
            if (!confirmed)
                return;
        }

        gameData.RecordSnapshot();
        replaceObjectKey(paramsObj, paramName, result.Name, nextDefinition);
        if (typeData["members"] is JsonObject members)
        {
            foreach (KeyValuePair<string, JsonNode?> memberEntry in members)
            {
                if (memberEntry.Value is not JsonObject member)
                    continue;
                JsonNode? nextValue = resetMemberValues
                    ? buildMemberDefaultValue(nextDefinition)
                    : member[paramName];
                replaceObjectKey(member, paramName, result.Name, nextValue);
            }
        }
        gameData.refreshModifiedState();
        populateMemberList(selectedMemberId);
    }

    private async Task onRemoveParamAsync(string paramName)
    {
        string msg = LocaleService.Get("CONFIRM_DELETE_PARAM").Replace("{}", paramName);
        bool confirmed = await ConfirmationDialog.ShowAsync(owner, LocaleService.Get("ADD_PARAM"), msg);
        if (!confirmed)
            return;
        gameData.RecordSnapshot();
        if (typeData["params"] is JsonObject paramsObj)
            paramsObj.Remove(paramName);
        if (typeData["members"] is JsonObject members)
        {
            foreach (KeyValuePair<string, JsonNode?> mEntry in members)
            {
                if (mEntry.Value is JsonObject member)
                    member.Remove(paramName);
            }
        }
        gameData.refreshModifiedState();
        buildForm(selectedMemberId);
    }

    private Control buildFieldEditor(string paramName, JsonObject paramDef, JsonNode? rawValue, JsonObject member)
    {
        string type = paramDef["type"]?.GetValue<string>() ?? "string";
        JsonObject? reference = getParamReference(paramDef);
        string refKind = reference?["kind"]?.GetValue<string>() ?? string.Empty;
        string refKey = reference?["key"]?.GetValue<string>() ?? string.Empty;

        if (type == "bool")
        {
            bool current = rawValue?.GetValue<bool?>() ?? false;
            CheckBox check = new() { IsChecked = current, VerticalAlignment = VerticalAlignment.Center };
            check.IsCheckedChanged += (_, _) =>
            {
                bool next = check.IsChecked ?? false;
                if ((member[paramName]?.GetValue<bool?>() ?? false) == next)
                    return;
                gameData.RecordSnapshot();
                member[paramName] = next;
                rawValue = member[paramName];
                gameData.refreshModifiedState();
            };
            return check;
        }

        if (type == "int")
        {
            int current = rawValue?.GetValue<int?>() ?? 0;
            NumericUpDown num = EditorInputs.CreateNumericUpDown(current, -999999, 999999, 1);
            HistoryMergeBehavior.Attach(num, gameData);
            num.ValueChanged += (_, _) =>
            {
                int next = (int)(num.Value ?? 0);
                if ((rawValue?.GetValue<int?>() ?? 0) == next)
                    return;
                gameData.RecordSnapshot();
                member[paramName] = next;
                rawValue = member[paramName];
                gameData.refreshModifiedState();
            };
            return num;
        }

        if (type == "float")
        {
            double current = rawValue?.GetValue<double?>() ?? 0.0;
            NumericUpDown num = EditorInputs.CreateNumericUpDown((decimal)current, -999999, 999999, 0.01m);
            HistoryMergeBehavior.Attach(num, gameData);
            num.ValueChanged += (_, _) =>
            {
                double next = (double)(num.Value ?? 0);
                if (Math.Abs((rawValue?.GetValue<double?>() ?? 0.0) - next) < 1e-10)
                    return;
                gameData.RecordSnapshot();
                member[paramName] = next;
                rawValue = member[paramName];
                gameData.refreshModifiedState();
            };
            return num;
        }

        if (type == "file")
        {
            string current = rawValue?.GetValue<string>() ?? string.Empty;
            TextBox pathBox = EditorInputs.CreateReadOnlyTextBox(current);
            Button browseBtn = new() { Content = "...", MinWidth = 36, Height = 34 };
            browseBtn.Click += async (_, _) =>
            {
                string baseHint = paramDef["base"]?.GetValue<string>() ?? string.Empty;
                if (!GameAssetPath.TryResolveBaseHint(
                        gameData.ProjectPath,
                        baseHint,
                        out string startDir))
                {
                    return;
                }
                Directory.CreateDirectory(startDir);
                string currentPath = pathBox.Text ?? string.Empty;
                string? initialFilePath = GameAssetPath.TryResolveExistingFile(
                    gameData.ProjectPath,
                    currentPath,
                    out string resolvedCurrent)
                    ? resolvedCurrent
                    : null;
                string? path = await FileSelectorDialog.ShowAsync(
                    owner,
                    startDir,
                    FileSelectorDialog.AllFilesFilter(),
                    initialFilePath: initialFilePath);
                if (path is null)
                    return;
                if (!GameAssetPath.TryFromProjectFile(
                        gameData.ProjectPath,
                        path,
                        out string assetPath)
                    || string.Equals(
                        rawValue?.GetValue<string>() ?? string.Empty,
                        assetPath,
                        StringComparison.Ordinal))
                {
                    return;
                }
                gameData.RecordSnapshot();
                member[paramName] = assetPath;
                rawValue = member[paramName];
                gameData.refreshModifiedState();
                pathBox.Text = assetPath;
            };
            Grid fileRow = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 4 };
            fileRow.Children.Add(pathBox);
            Grid.SetColumn(browseBtn, 1);
            fileRow.Children.Add(browseBtn);
            return fileRow;
        }

        if (type == "list")
        {
            JsonArray current = rawValue is JsonArray arr ? (JsonArray)arr.DeepClone() : new JsonArray();
            List<string>? refOptions = getRefOptions(refKind, refKey);
            return buildTypedFieldEditor(paramName, paramDef, current, member, refOptions);
        }

        if (type == "dict")
        {
            JsonObject current = rawValue is JsonObject obj ? (JsonObject)obj.DeepClone() : new JsonObject();
            List<string>? refOptions = getRefOptions(refKind, refKey);
            return buildTypedFieldEditor(paramName, paramDef, current, member, refOptions);
        }

        if (type.StartsWith("tuple", StringComparison.Ordinal) &&
            System.Text.RegularExpressions.Regex.Match(type, @"tuple\[(\d+)\]") is { Success: true } m)
        {
            int size = int.Parse(m.Groups[1].Value);
            JsonArray tupleVal = rawValue is JsonArray ta ? (JsonArray)ta.DeepClone() : new JsonArray();
            while (tupleVal.Count < size)
                tupleVal.Add(string.Empty);
            Grid tupleRow = new()
            {
                ColumnDefinitions = new ColumnDefinitions(string.Join(",", Enumerable.Repeat("*", size))),
                ColumnSpacing = 4,
            };
            for (int i = 0; i < size; i++)
            {
                int captured = i;
                TextBox box = EditorInputs.CreateEditableTextBox(tupleVal[captured]?.GetValue<string>() ?? string.Empty);
                HistoryMergeBehavior.Attach(box, gameData);
                box.TextChanged += (_, _) =>
                {
                    gameData.RecordSnapshot();
                    while (tupleVal.Count <= captured)
                        tupleVal.Add(string.Empty);
                    tupleVal[captured] = box.Text ?? string.Empty;
                    member[paramName] = tupleVal;
                    gameData.refreshModifiedState();
                };
                Grid.SetColumn(box, captured);
                tupleRow.Children.Add(box);
            }
            return tupleRow;
        }

        if (isSfType(type))
            return buildTypedFieldEditor(paramName, paramDef, rawValue, member, null);

        {
            string current = rawValue?.GetValue<string>() ?? string.Empty;
            List<string>? refOptions = getRefOptions(refKind, refKey);
            if (refOptions is not null)
            {
                ComboBox combo = GeneralDataReferenceInputs.Create(current, refOptions);
                combo.SelectionChanged += (_, _) =>
                {
                    string next = GeneralDataReferenceInputs.GetValue(combo);
                    if ((rawValue?.GetValue<string>() ?? string.Empty) == next)
                        return;
                    gameData.RecordSnapshot();
                    member[paramName] = next;
                    rawValue = member[paramName];
                    gameData.refreshModifiedState();
                };
                return combo;
            }
            TextBox box = EditorInputs.CreateEditableTextBox(current);
            HistoryMergeBehavior.Attach(box, gameData);
            box.TextChanged += (_, _) =>
            {
                string next = box.Text ?? string.Empty;
                if ((rawValue?.GetValue<string>() ?? string.Empty) == next)
                    return;
                gameData.RecordSnapshot();
                member[paramName] = next;
                rawValue = member[paramName];
                gameData.refreshModifiedState();
            };
            return box;
        }
    }

    private Control buildTypedFieldEditor(
        string paramName,
        JsonObject paramDef,
        JsonNode? rawValue,
        JsonObject member,
        IReadOnlyList<string>? referenceOptions)
    {
        string type = paramDef["type"]?.GetValue<string>() ?? "string";
        string editorType = type switch
        {
            "list" => getContainerItemType(paramDef, "itemType") + "[]",
            "dict" => "Dict[string, " + getContainerItemType(paramDef, "valueType") + "]",
            _ => type,
        };
        JsonObject meta = buildTypedFieldMeta(type, paramDef, referenceOptions);
        BlueprintVariableField field = new(paramName, editorType, rawValue)
        {
            Meta = meta,
        };
        BlueprintVariableForm form = new()
        {
            AssetsDirectory = Path.Combine(gameData.ProjectPath, "Assets"),
            ProjectDirectory = gameData.ProjectPath,
            CellSize = gameData.getCellSize(),
            HistoryGameData = gameData,
            ShowFieldNames = false,
            CustomValueEditorFactory = createGeneralDataReferenceEditor,
        };
        form.ValueChanged += (_, args) =>
        {
            if (JsonNode.DeepEquals(member[paramName], args.Value))
                return;
            gameData.RecordSnapshot();
            member[paramName] = args.Value?.DeepClone();
            gameData.refreshModifiedState();
        };
        form.SetFields([field]);
        return form;
    }

    private static JsonObject buildTypedFieldMeta(
        string type,
        JsonObject paramDef,
        IReadOnlyList<string>? referenceOptions)
    {
        JsonObject meta = [];
        if (type == "list")
        {
            JsonObject itemMeta = [];
            if (getContainerItemType(paramDef, "itemType") == "file")
                itemMeta["PathVars"] = GameAssetPath.Root;
            if (referenceOptions is not null)
                itemMeta["GeneralDataReference"] = buildReferenceOptions(referenceOptions);
            if (itemMeta.Count > 0)
                meta["ItemMeta"] = itemMeta;
        }
        else if (type == "dict")
        {
            if (getContainerItemType(paramDef, "valueType") == "file")
                meta["ItemMeta"] = new JsonObject { ["PathVars"] = GameAssetPath.Root };
            if (referenceOptions is not null)
            {
                meta["DictKeyMeta"] = new JsonObject
                {
                    ["GeneralDataReference"] = buildReferenceOptions(referenceOptions),
                };
            }
        }
        return meta;
    }

    private static JsonArray buildReferenceOptions(IEnumerable<string> options)
    {
        JsonArray result = [];
        foreach (string option in options)
            result.Add(option);
        return result;
    }

    private static Control? createGeneralDataReferenceEditor(BlueprintVariableEditorRequest request)
    {
        if (request.Field.Meta["GeneralDataReference"] is not JsonArray rawOptions)
            return null;
        List<string> options = rawOptions
            .Select(option => option?.GetValue<string>() ?? string.Empty)
            .ToList();
        string current = request.Value?.GetValue<string>() ?? string.Empty;
        ComboBox combo = GeneralDataReferenceInputs.Create(current, options);
        combo.SelectionChanged += (_, _) =>
        {
            string next = GeneralDataReferenceInputs.GetValue(combo);
            request.Commit(JsonValue.Create(next), false);
        };
        return combo;
    }

    private static string getContainerItemType(JsonObject paramDef, string name)
    {
        string? value = paramDef[name]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(value))
            throw new InvalidOperationException($"General Data container is missing {name}");
        return value;
    }

    private static bool isSfType(string type)
    {
        return type.StartsWith("sf.", StringComparison.Ordinal);
    }

    private List<string>? getRefOptions(string refKind, string refKey)
    {
        if (refKind == "animation")
            return gameData.AnimationsData.Keys.OrderBy(k => k, StringComparer.Ordinal).ToList();
        if (refKind == "general" && !string.IsNullOrEmpty(refKey)
            && gameData.GeneralData.TryGetValue(refKey, out JsonObject? refData)
            && refData["members"] is JsonObject refMembers)
            return refMembers.Select(e => e.Key).OrderBy(k => k, StringComparer.Ordinal).ToList();
        if (refKind == "general" && !string.IsNullOrEmpty(refKey))
            return new List<string>();
        return null;
    }

    private JsonObject buildDefaultMember()
    {
        JsonObject member = new();
        if (typeData["params"] is JsonObject paramsObj)
        {
            foreach (KeyValuePair<string, JsonNode?> entry in paramsObj)
            {
                if (entry.Value is not JsonObject paramDef)
                    continue;
                member[entry.Key] = buildMemberDefaultValue(paramDef);
            }
        }
        return member;
    }

    private static JsonNode? buildMemberDefaultValue(JsonObject paramDef)
    {
        string type = paramDef["type"]?.GetValue<string>() ?? "string";
        JsonNode? defaultDef = paramDef["defaultValue"];
        return type switch
        {
            "int" => defaultDef?.GetValue<int?>() ?? 0,
            "float" => defaultDef?.GetValue<double?>() ?? 0.0,
            "bool" => defaultDef?.GetValue<bool?>() ?? false,
            "list" => defaultDef is JsonArray arr ? (JsonArray)arr.DeepClone() : new JsonArray(),
            "dict" => defaultDef is JsonObject obj ? (JsonObject)obj.DeepClone() : new JsonObject(),
            "file" => JsonValue.Create(string.Empty),
            _ when isSfType(type) => createTypedDefault(type),
            _ => JsonValue.Create(defaultDef?.GetValue<string>() ?? string.Empty),
        };
    }

    private static JsonNode createTypedDefault(string type)
    {
        return LuaMetadataValueDefaults.Create(
            LuaMetadataType.Parse(type),
            _ => JsonValue.Create(string.Empty)) ?? JsonValue.Create(string.Empty)!;
    }

    private static JsonObject buildParamDefinition(GeneralDataParamCreation value)
    {
        JsonObject definition = new()
        {
            ["type"] = value.Type,
            ["defaultValue"] = value.Type == "file"
                ? JsonValue.Create(string.Empty)
                : parseDefaultValue(value.Type, value.DefaultText),
        };
        if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
            definition["base"] = value.DefaultText.Trim();
        if (value.Comment.Length > 0)
            definition["comment"] = value.Comment;
        if (value.ItemType is not null)
            definition["itemType"] = value.ItemType;
        if (value.ValueType is not null)
            definition["valueType"] = value.ValueType;
        return definition;
    }

    private static JsonObject updateParamDefinition(
        JsonObject currentDefinition,
        GeneralDataParamCreation value)
    {
        JsonObject definition = (JsonObject)currentDefinition.DeepClone();
        definition["type"] = value.Type;
        definition["defaultValue"] = value.Type == "file"
            ? JsonValue.Create(string.Empty)
            : parseDefaultValue(value.Type, value.DefaultText);
        if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
            definition["base"] = value.DefaultText.Trim();
        else
            definition.Remove("base");
        if (value.Comment.Length == 0)
            definition.Remove("comment");
        else
            definition["comment"] = value.Comment;
        if (value.ItemType is null)
            definition.Remove("itemType");
        else
            definition["itemType"] = value.ItemType;
        if (value.ValueType is null)
            definition.Remove("valueType");
        else
            definition["valueType"] = value.ValueType;
        if (!isParamReferenceAllowed(definition))
            definition.Remove("reference");
        return definition;
    }

    private static GeneralDataParamCreation createParamCreation(
        string name,
        JsonObject definition)
    {
        string type = definition["type"]?.GetValue<string>() ?? "string";
        return new GeneralDataParamCreation(
            name,
            type,
            type == "list" ? getContainerItemType(definition, "itemType") : null,
            type == "dict" ? getContainerItemType(definition, "valueType") : null,
            type == "file"
                ? definition["base"]?.GetValue<string>() ?? string.Empty
                : formatDefaultValue(type, definition["defaultValue"]),
            definition["comment"]?.GetValue<string>() ?? string.Empty);
    }

    private static bool hasValueTypeChanged(
        GeneralDataParamCreation current,
        GeneralDataParamCreation next)
    {
        if (current.Type != next.Type)
            return true;
        return current.Type switch
        {
            "list" => current.ItemType != next.ItemType,
            "dict" => current.ValueType != next.ValueType,
            _ => false,
        };
    }

    private static string formatDefaultValue(string type, JsonNode? value)
    {
        return type switch
        {
            "int" => (value?.GetValue<int?>() ?? 0).ToString(CultureInfo.InvariantCulture),
            "float" => (value?.GetValue<double?>() ?? 0.0).ToString(CultureInfo.InvariantCulture),
            "bool" => value?.GetValue<bool?>() == true ? "true" : "false",
            "list" or "dict" => string.Empty,
            _ when isSfType(type) => string.Empty,
            _ => value?.GetValue<string>() ?? string.Empty,
        };
    }

    private static void reorderMemberKey(JsonObject members, string oldKey, string newKey)
    {
        replaceObjectKey(members, oldKey, newKey, members[oldKey]);
    }

    private static void replaceObjectKey(
        JsonObject obj,
        string oldKey,
        string newKey,
        JsonNode? newValue)
    {
        List<KeyValuePair<string, JsonNode?>> entries = obj.Select(entry => entry).ToList();
        obj.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            obj.Add(entry.Key == oldKey ? newKey : entry.Key, entry.Key == oldKey ? newValue : entry.Value);
    }

    private static JsonNode parseDefaultValue(string type, string text)
    {
        return type switch
        {
            "int" => int.TryParse(text, out int i) ? i : 0,
            "float" => double.TryParse(text, out double d) ? d : 0.0,
            "bool" => text.Equals("true", StringComparison.OrdinalIgnoreCase) ? true : false,
            "list" => new JsonArray(),
            "dict" => new JsonObject(),
            "file" => JsonValue.Create(string.Empty)!,
            _ when isSfType(type) => createTypedDefault(type),
            _ => JsonValue.Create(text) ?? JsonValue.Create(string.Empty)!,
        };
    }
}

internal sealed record GeneralDataTableColumn(string Name, JsonObject Definition);

internal sealed record GeneralDataTableRow(string Id, JsonObject Member);

internal static class GeneralDataReferenceInputs
{
    public static ComboBox Create(string current, IReadOnlyList<string> options)
    {
        ComboBox combo = new()
        {
            MinHeight = EditorInputs.FieldMinHeight,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        ComboBoxItem placeholder = createItem(LocaleService.Get("GENERAL_DATA_PLACEHOLDER"), string.Empty);
        combo.Items.Add(placeholder);
        ComboBoxItem selected = placeholder;
        if (current.Length > 0 && !options.Contains(current))
        {
            selected = createItem(current, current);
            combo.Items.Add(selected);
        }
        foreach (string option in options)
        {
            if (option.Length == 0)
                continue;
            ComboBoxItem item = createItem(option, option);
            combo.Items.Add(item);
            if (string.Equals(option, current, StringComparison.Ordinal))
                selected = item;
        }
        combo.SelectedItem = selected;
        return combo;
    }

    public static string GetValue(ComboBox combo)
    {
        return (combo.SelectedItem as ComboBoxItem)?.Tag as string ?? string.Empty;
    }

    private static ComboBoxItem createItem(string label, string value)
    {
        return new ComboBoxItem
        {
            Content = label,
            Tag = value,
        };
    }
}
