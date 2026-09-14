using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
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

internal sealed class GeneralDataPage : Grid
{
    private const double TableIdColumnWidth = 180;
    private const double TableFieldColumnWidth = 200;
    private readonly GeneralDataEditorWindow owner;
    private readonly GameDataService gameData;
    private readonly EditorDocument? resourceDocument;
    private readonly string initialTypeKey;
    private string typeKey => resourceDocument?.Key ?? initialTypeKey;
    private bool attached;
    private bool refreshPending;
    private bool searchPending;
    private long documentRevision;
    private int formVersion;
    private bool formInterrupted;
    private bool formBuildPending;
    private Vector? preservedFormOffset;
    private int restoreFormVersion = -1;
    private JsonObject typeData;
    private readonly Dictionary<JsonObject, string> memberKeys = [];
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
        initialTypeKey = typeKey;
        resourceDocument = gameData.GetDocument("General", typeKey);
        this.typeData = typeData;
        documentRevision = resourceDocument?.Revision ?? 0;
        rebuildMemberKeys();
        this.sessionState = sessionState;

        ColumnDefinitions = new ColumnDefinitions("240,4,*");

        Grid leftGrid = new() { RowDefinitions = new RowDefinitions("Auto,*,Auto") };
        searchBox = EditorInputs.CreateEditableTextBox(sessionState.SearchText);
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.Margin = new Thickness(4);
        searchBox.TextChanged += (_, _) =>
        {
            string next = searchBox.Text ?? string.Empty;
            if (sessionState.SearchText == next)
                return;
            sessionState.SearchText = next;
            if (searchPending)
                return;
            searchPending = true;
            Dispatcher.UIThread.Post(() =>
            {
                searchPending = false;
                if (attached)
                    populateMemberList(sessionState.SelectedMemberId, false);
            }, DispatcherPriority.Background);
        };
        Grid.SetRow(searchBox, 0);
        leftGrid.Children.Add(searchBox);

        memberList = new ListBox
        {
            Background = Ludork.Services.EditorTheme.Brush("Surface"),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        memberList.SelectionChanged += onMemberSelectionChanged;
        memberList.AddHandler(ContextRequestedEvent, onMemberListContextRequested, RoutingStrategies.Bubble);
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
            Background = Ludork.Services.EditorTheme.Brush("Input"),
            VerticalAlignment = VerticalAlignment.Stretch,
        };
        Grid.SetColumn(splitter, 1);
        Children.Add(splitter);

        Grid rightGrid = new() { RowDefinitions = new RowDefinitions("Auto,*") };
        actionBar = new Border
        {
            Background = Ludork.Services.EditorTheme.Brush("Surface"),
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
            Background = Ludork.Services.EditorTheme.Brush("Input"),
        };
        tableList = new ListBox
        {
            Background = Ludork.Services.EditorTheme.Brush("Surface"),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = new FuncDataTemplate<GeneralDataTableRow>(buildTableRow),
        };
        tableList.SelectionChanged += onTableSelectionChanged;
        tableList.AddHandler(ContextRequestedEvent, onTableContextRequested, RoutingStrategies.Tunnel);
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
        updateViewMode(false);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        if (formInterrupted && documentRevision == (resourceDocument?.Revision ?? 0)
            && sessionState.ViewMode == GeneralDataViewMode.Form)
        {
            formInterrupted = false;
            Vector offset = preservedFormOffset ?? formScroll.Offset;
            buildForm(selectedMemberId);
            restoreFormOffset(offset);
        }
        if (resourceDocument is not null)
            resourceDocument.Changed += onDocumentChanged;
        onDocumentChanged(this, EventArgs.Empty);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
        formVersion++;
        formInterrupted |= formBuildPending;
        formBuildPending = false;
        if (resourceDocument is not null)
            resourceDocument.Changed -= onDocumentChanged;
        base.OnDetachedFromVisualTree(args);
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (refreshPending)
            return;
        refreshPending = true;
        Dispatcher.UIThread.Post(() =>
        {
            refreshPending = false;
            if (!attached || resourceDocument is null || documentRevision == resourceDocument.Revision
                || resourceDocument.Data is not JsonObject current)
                return;
            documentRevision = resourceDocument.Revision;
            if (JsonNode.DeepEquals(current, typeData))
                return;
            typeData = current;
            rebuildMemberKeys();
            Vector formOffset = preservedFormOffset ?? formScroll.Offset;
            Vector tableOffset = tableScroll.Offset;
            populateMemberList(sessionState.SelectedMemberId);
            updateActionBar();
            restoreFormOffset(formOffset);
            tableScroll.Offset = tableOffset;
        });
    }

    private void populateMemberList(string? preferredMemberId = null, bool refreshContent = true)
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
        if (!memberIds.SequenceEqual(memberList.ItemsSource?.Cast<string>() ?? [], StringComparer.Ordinal))
            memberList.ItemsSource = memberIds;
        memberList.SelectedItem = nextSelection;
        syncingSelection = false;
        selectMember(nextSelection, refreshContent);
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

    private void selectMember(string? memberId, bool refreshContent = true)
    {
        bool selectionChanged = selectedMemberId != memberId;
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
        if (sessionState.ViewMode == GeneralDataViewMode.Form && (selectionChanged || refreshContent || memberId is null))
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

    private void updateViewMode(bool refreshContent = true)
    {
        bool showForm = sessionState.ViewMode == GeneralDataViewMode.Form;
        formScroll.IsVisible = showForm;
        tableScroll.IsVisible = !showForm;
        formViewButton.IsEnabled = !showForm;
        tableViewButton.IsEnabled = showForm;
        if (!refreshContent)
            return;
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
            BorderBrush = Ludork.Services.EditorTheme.Brush("Border"),
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
            BorderBrush = Ludork.Services.EditorTheme.Brush("Border"),
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
        string type = readTypeName(column.Definition["type"]);
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
                updateMemberValue(row.Member, column.Name, JsonValue.Create(next));
            };
            return check;
        }
        if (type is "int" or "float")
            return buildTypedFieldEditor(column.Name, column.Definition, rawValue, row.Member, null);
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
                    updateMemberValue(row.Member, column.Name, JsonValue.Create(next));
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
                updateMemberValue(row.Member, column.Name, JsonValue.Create(next));
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
        updateViewMode(false);
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

    private void onTableContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        bool requestedByPointer = args.TryGetPosition(this, out _);
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
        if (!requestedByPointer)
            hitId ??= (tableList.SelectedItem as GeneralDataTableRow)?.Id;
        if (hitId is not null)
            selectMember(hitId);
        args.Handled = true;
        showMemberContextMenu(hitId, tableList, requestedByPointer);
    }

    private void onMemberListContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        bool requestedByPointer = args.TryGetPosition(this, out _);
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
        if (!requestedByPointer)
            hitId ??= memberList.SelectedItem as string;
        if (hitId is not null)
            selectMember(hitId);
        args.Handled = true;
        showMemberContextMenu(hitId, memberList, requestedByPointer);
    }

    private void showMemberContextMenu(string? memberId, ListBox list, bool requestedByPointer)
    {
        if (memberId is null)
            return;
        ContextMenu menu = new() { Placement = requestedByPointer ? PlacementMode.Pointer : PlacementMode.Bottom };
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
        Control anchor = list.ContainerFromIndex(list.SelectedIndex) ?? (Control)list;
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
        if (!gameData.CreateGeneralMember(typeKey, id))
            return;
        reloadTypeData();
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
        if (!gameData.RenameGeneralMember(typeKey, oldId, newId))
            return;
        reloadTypeData();
        revealMember(newId);
    }

    private async Task onDuplicateMemberAsync(string sourceId)
    {
        JsonObject? members = typeData["members"] as JsonObject;
        if (members is null || members[sourceId] is not JsonObject)
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
        if (!gameData.DuplicateGeneralMember(typeKey, sourceId, confirmedId))
            return;
        reloadTypeData();
        revealMember(confirmedId);
    }

    private void onRemoveMember(string memberId)
    {
        JsonObject? members = typeData["members"] as JsonObject;
        if (members is null)
            return;
        owner.closeBlueprintEditor(typeKey, memberId);
        if (!gameData.DeleteGeneralMember(typeKey, memberId))
            return;
        reloadTypeData();
        populateMemberList();
    }

    private void buildForm(string? memberId)
    {
        int version = ++formVersion;
        preservedFormOffset = null;
        restoreFormVersion = -1;
        formBuildPending = true;
        formInterrupted = false;
        formContent.Children.Clear();
        IEnumerator<Control> rows = createFormRows(memberId).GetEnumerator();
        void appendRows()
        {
            if (version != formVersion)
            {
                rows.Dispose();
                return;
            }
            System.Diagnostics.Stopwatch clock = System.Diagnostics.Stopwatch.StartNew();
            while (rows.MoveNext())
            {
                formContent.Children.Add(rows.Current);
                if (clock.Elapsed.TotalMilliseconds >= 8)
                {
                    Dispatcher.UIThread.Post(appendRows, DispatcherPriority.Background);
                    return;
                }
            }
            rows.Dispose();
            formBuildPending = false;
            scheduleFormOffsetRestore();
        }
        appendRows();
    }

    private void restoreFormOffset(Vector offset)
    {
        preservedFormOffset = offset;
        restoreFormVersion = formVersion;
        if (!formBuildPending)
            scheduleFormOffsetRestore();
    }

    private void scheduleFormOffsetRestore()
    {
        if (preservedFormOffset is not Vector offset || restoreFormVersion != formVersion)
            return;
        int version = formVersion;
        Dispatcher.UIThread.Post(() =>
        {
            if (!attached || version != formVersion || restoreFormVersion != version || formBuildPending)
                return;
            formScroll.Offset = offset;
            preservedFormOffset = null;
            restoreFormVersion = -1;
        }, DispatcherPriority.Background);
    }

    private IEnumerable<Control> createFormRows(string? memberId)
    {
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
                Foreground = Ludork.Services.EditorTheme.Brush("TextMuted"),
            };
            yield return placeholder;
            yield break;
        }

        JsonObject? paramsObj = typeData["params"] as JsonObject;

        TextBox idBox = EditorInputs.CreateReadOnlyTextBox(memberId);
        yield return buildFormRow("ID", idBox, null, null);

        if (paramsObj is not null)
        {
            foreach (KeyValuePair<string, JsonNode?> paramEntry in paramsObj.ToArray())
            {
                if (paramEntry.Value is not JsonObject paramDef)
                    continue;
                string paramName = paramEntry.Key;
                JsonNode? rawValue = member[paramName];
                Control editor = buildFieldEditor(paramName, paramDef, rawValue, member);
                Control row = buildFormRow(paramName, editor, paramsObj, paramName);
                yield return row;
            }
        }

        Button addParamBtn = new()
        {
            Content = "+ " + LocaleService.Get("ADD_PARAM"),
            HorizontalAlignment = HorizontalAlignment.Left,
            Margin = new Thickness(0, 6, 0, 0),
        };
        addParamBtn.Click += async (_, _) => await onAddParamAsync(memberId);
        yield return addParamBtn;
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
        string? name = (typeData["params"] as JsonObject)?.FirstOrDefault(
            entry => ReferenceEquals(entry.Value, paramDef)).Key;
        if (name is null || !gameData.UpdateGeneralParameterReference(typeKey, name, reference))
            return;
        reloadTypeData();
        buildForm(selectedMemberId);
    }

    private static bool isParamReferenceAllowed(JsonObject? paramDef)
    {
        if (paramDef is null)
            return false;
        string type = readTypeName(paramDef["type"]);
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
        if (!gameData.AddGeneralParameter(typeKey, result.Name, paramDef))
            return;
        reloadTypeData();
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

        JsonObject nextDefinition = updateParamDefinition(currentDefinition, initialValue, result);
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

        if (!gameData.UpdateGeneralParameter(typeKey, paramName, result.Name, nextDefinition, resetMemberValues))
            return;
        reloadTypeData();
        populateMemberList(selectedMemberId);
    }

    private async Task onRemoveParamAsync(string paramName)
    {
        string msg = LocaleService.Get("CONFIRM_DELETE_PARAM").Replace("{}", paramName);
        bool confirmed = await ConfirmationDialog.ShowAsync(owner, LocaleService.Get("ADD_PARAM"), msg);
        if (!confirmed)
            return;
        if (!gameData.DeleteGeneralParameter(typeKey, paramName))
            return;
        reloadTypeData();
        buildForm(selectedMemberId);
    }

    private Control buildFieldEditor(string paramName, JsonObject paramDef, JsonNode? rawValue, JsonObject member)
    {
        string type = readTypeName(paramDef["type"]);
        if (LuaMetadataType.Parse(type).Kind == LuaMetadataTypeKind.Union
            || type.StartsWith("Tuple[", StringComparison.Ordinal)
            || paramDef["type"] is JsonObject)
        {
            return buildTypedFieldEditor(paramName, paramDef, rawValue, member, null);
        }
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
                if (!updateMemberValue(member, paramName, JsonValue.Create(next)))
                    return;
                rawValue = member[paramName];
            };
            return check;
        }

        if (type is "int" or "float")
            return buildTypedFieldEditor(paramName, paramDef, rawValue, member, null);

        if (type == "file")
        {
            string current = rawValue?.GetValue<string>() ?? string.Empty;
            TextBox pathBox = EditorInputs.CreateReadOnlyTextBox(current);
            Button browseBtn = new() { Content = "...", MinWidth = 36, Height = EditorInputs.FieldMinHeight };
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
                if (!updateMemberValue(member, paramName, JsonValue.Create(assetPath)))
                    return;
                rawValue = member[paramName];
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
                    while (tupleVal.Count <= captured)
                        tupleVal.Add(string.Empty);
                    tupleVal[captured] = box.Text ?? string.Empty;
                    updateMemberValue(member, paramName, tupleVal);
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
                    if (!updateMemberValue(member, paramName, JsonValue.Create(next)))
                        return;
                    rawValue = member[paramName];
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
                if (!updateMemberValue(member, paramName, JsonValue.Create(next)))
                    return;
                rawValue = member[paramName];
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
        string type = readTypeName(paramDef["type"]);
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
        form.SetFields([field]);
        form.ValueChanged += (_, args) =>
        {
            if (JsonNode.DeepEquals(member[paramName], args.Value))
                return;
            updateMemberValue(member, paramName, args.Value);
        };
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
        string? value = paramDef[name] is null ? null : readTypeName(paramDef[name]);
        if (string.IsNullOrWhiteSpace(value))
            throw new InvalidOperationException($"General Data container is missing {name}");
        return value;
    }

    private static string readTypeName(JsonNode? type)
    {
        if (type is JsonValue scalar && scalar.TryGetValue(out string? text))
            return text ?? "string";
        return type is null ? "string" : LuaMetadataType.Parse(type).ToString();
    }

    private static JsonNode canonicalTypeNode(string type)
    {
        return type is "list" or "dict" ? JsonValue.Create(type)! : LuaMetadataType.Parse(type).ToSchema();
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

    private bool updateMemberValue(JsonObject member, string name, JsonNode? value)
    {
        if (!memberKeys.TryGetValue(member, out string? memberId)
            || !gameData.UpdateGeneralMemberValue(typeKey, memberId, name, value))
            return false;
        member[name] = value?.DeepClone();
        return true;
    }

    private void reloadTypeData()
    {
        typeData = gameData.GeneralData.TryGetValue(typeKey, out JsonObject? current) ? current : [];
        rebuildMemberKeys();
    }

    private void rebuildMemberKeys()
    {
        memberKeys.Clear();
        if (typeData["members"] is not JsonObject members)
            return;
        foreach (KeyValuePair<string, JsonNode?> entry in members)
        {
            if (entry.Value is JsonObject member)
                memberKeys[member] = entry.Key;
        }
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
            ["type"] = canonicalTypeNode(value.Type),
            ["defaultValue"] = value.Type == "file"
                ? JsonValue.Create(string.Empty)
                : parseDefaultValue(value.Type, value.DefaultText),
        };
        if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
            definition["base"] = value.DefaultText.Trim();
        if (value.Comment.Length > 0)
            definition["comment"] = value.Comment;
        if (value.ItemType is not null)
            definition["itemType"] = canonicalTypeNode(value.ItemType);
        if (value.ValueType is not null)
            definition["valueType"] = canonicalTypeNode(value.ValueType);
        return definition;
    }

    private static JsonObject updateParamDefinition(
        JsonObject currentDefinition,
        GeneralDataParamCreation initialValue,
        GeneralDataParamCreation value)
    {
        JsonObject definition = (JsonObject)currentDefinition.DeepClone();
        bool typeChanged = hasValueTypeChanged(initialValue, value);
        if (initialValue.Type != value.Type)
            definition["type"] = canonicalTypeNode(value.Type);
        if (typeChanged || initialValue.DefaultText != value.DefaultText)
        {
            if (typeChanged || value.Type != "file")
            {
                definition["defaultValue"] = value.Type == "file"
                    ? JsonValue.Create(string.Empty)
                    : parseDefaultValue(value.Type, value.DefaultText);
            }
            if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
                definition["base"] = value.DefaultText.Trim();
            else if (initialValue.Type == "file" || value.Type == "file")
                definition.Remove("base");
        }
        if (initialValue.Comment.Trim() != value.Comment)
        {
            if (value.Comment.Length == 0)
                definition.Remove("comment");
            else
                definition["comment"] = value.Comment;
        }
        if (initialValue.ItemType != value.ItemType)
        {
            if (value.ItemType is null)
                definition.Remove("itemType");
            else
                definition["itemType"] = canonicalTypeNode(value.ItemType);
        }
        if (initialValue.ValueType != value.ValueType)
        {
            if (value.ValueType is null)
                definition.Remove("valueType");
            else
                definition["valueType"] = canonicalTypeNode(value.ValueType);
        }
        if (typeChanged && !isParamReferenceAllowed(definition))
            definition.Remove("reference");
        return definition;
    }

    private static GeneralDataParamCreation createParamCreation(
        string name,
        JsonObject definition)
    {
        string type = readTypeName(definition["type"]);
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
            "int" => (value?.GetValue<long?>() ?? 0).ToString(CultureInfo.InvariantCulture),
            "float" => (value?.GetValue<double?>() ?? 0.0).ToString(CultureInfo.InvariantCulture),
            "bool" => value?.GetValue<bool?>() == true ? "true" : "false",
            "list" or "dict" => string.Empty,
            _ when isSfType(type) => string.Empty,
            _ when LuaMetadataType.Parse(type).Kind != LuaMetadataTypeKind.Named => value?.ToJsonString() ?? "null",
            _ => value?.GetValue<string>() ?? string.Empty,
        };
    }

    private static JsonNode parseDefaultValue(string type, string text)
    {
        return type switch
        {
            "int" => long.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out long i) ? i : 0,
            "float" => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double d) ? d : 0.0,
            "bool" => text.Equals("true", StringComparison.OrdinalIgnoreCase) ? true : false,
            "list" => new JsonArray(),
            "dict" => new JsonObject(),
            _ when LuaMetadataType.Parse(type).Kind != LuaMetadataTypeKind.Named => JsonNode.Parse(text)!,
            "file" => JsonValue.Create(string.Empty)!,
            _ when isSfType(type) => createTypedDefault(type),
            _ => JsonValue.Create(text) ?? JsonValue.Create(string.Empty)!,
        };
    }
}
