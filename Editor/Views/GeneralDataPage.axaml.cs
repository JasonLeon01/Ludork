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
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed partial class GeneralDataPage : UserControl
{
    private const double TableIdColumnWidth = 180;
    private const double TableFieldColumnWidth = 200;
    private readonly GeneralDataEditorWindow owner;
    private readonly ProjectDataStore gameData;
    private readonly GeneralDataPageViewModel viewModel;
    private string typeKey => viewModel.TypeKey;
    private bool attached;
    private bool refreshPending;
    private bool searchPending;
    private int formVersion;
    private bool formInterrupted;
    private bool formBuildPending;
    private Vector? preservedFormOffset;
    private int restoreFormVersion = -1;
    private readonly TextBox searchBox;
    private readonly ListBox memberList;
    private readonly ScrollViewer formScroll;
    private readonly StackPanel formContent;
    private readonly ScrollViewer tableScroll;
    private readonly Grid tableSurface;
    private readonly Grid tableHeader;
    private readonly ListBox tableList;
    private IReadOnlyList<GeneralDataTableColumn> tableColumns = [];

    private string? selectedMemberId => viewModel.SelectedMemberId;
    private string? renderedMemberId;
    private bool syncingViewMode;
    private bool syncingSelection;

    public GeneralDataPage(
        GeneralDataEditorWindow owner,
        ProjectDataStore gameData,
        string typeKey,
        GeneralDataTypeSnapshot typeData,
        GeneralDataPageSessionState sessionState)
    {
        this.owner = owner;
        this.gameData = gameData;
        viewModel = new GeneralDataPageViewModel(gameData.General, gameData.GetDocument("General", typeKey), typeKey, typeData, sessionState);
        InitializeComponent();
        DataContext = viewModel;
        searchBox = this.FindControl<TextBox>("SearchBox")!;
        memberList = this.FindControl<ListBox>("MemberList")!;
        formScroll = this.FindControl<ScrollViewer>("FormScroll")!;
        formContent = this.FindControl<StackPanel>("FormContent")!;
        tableScroll = this.FindControl<ScrollViewer>("TableScroll")!;
        tableSurface = this.FindControl<Grid>("TableSurface")!;
        tableHeader = this.FindControl<Grid>("TableHeader")!;
        tableList = this.FindControl<ListBox>("TableList")!;
        EditorInputs.ApplyEditable(searchBox);
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        memberList.ItemTemplate = HintedTextPresenter.StringItemTemplate;
        memberList.SelectionChanged += onMemberSelectionChanged;
        memberList.AddHandler(ContextRequestedEvent, onMemberListContextRequested, RoutingStrategies.Bubble);
        tableList.ItemTemplate = new FuncDataTemplate<GeneralDataTableRow>(buildTableRow);
        tableList.SelectionChanged += onTableSelectionChanged;
        tableList.AddHandler(ContextRequestedEvent, onTableContextRequested, RoutingStrategies.Tunnel);
        Button addMemberButton = this.FindControl<Button>("AddMemberButton")!;
        addMemberButton.Content = "+ " + LocaleService.Get("NEW_MEMBER");
        addMemberButton.Click += async (_, _) => await onAddMemberAsync();
        Button editAbilityGraphButton = this.FindControl<Button>("EditAbilityGraphButton")!;
        editAbilityGraphButton.Content = LocaleService.Get("EDIT_ABILITY_GRAPH");
        editAbilityGraphButton.Click += (_, _) => openSelectedAbilityGraph();
        this.FindControl<Button>("FormViewButton")!.Content = LocaleService.Get("GENERAL_DATA_FORM_VIEW");
        this.FindControl<Button>("TableViewButton")!.Content = LocaleService.Get("GENERAL_DATA_TABLE_VIEW");
        viewModel.PropertyChanged += onViewModelChanged;
        viewModel.DocumentChanged += onDocumentChanged;
        populateMemberList(viewModel.SelectedMemberId);
    }

    private void onViewModelChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(GeneralDataPageViewModel.ViewMode) && !syncingViewMode)
            updateViewMode();
        if (args.PropertyName != nameof(GeneralDataPageViewModel.SearchText) || searchPending)
            return;
        searchPending = true;
        Dispatcher.UIThread.Post(() =>
        {
            searchPending = false;
            if (attached)
                populateMemberList(viewModel.SelectedMemberId, false);
        }, DispatcherPriority.Background);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        if (formInterrupted && viewModel.DocumentRevision == viewModel.CurrentDocumentRevision
            && viewModel.ViewMode == GeneralDataViewMode.Form)
        {
            formInterrupted = false;
            Vector offset = preservedFormOffset ?? formScroll.Offset;
            buildForm(selectedMemberId);
            restoreFormOffset(offset);
        }
        viewModel.Activate();
        onDocumentChanged(this, EventArgs.Empty);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
        formVersion++;
        formInterrupted |= formBuildPending;
        formBuildPending = false;
        viewModel.Deactivate();
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
            if (!attached || !viewModel.RefreshFromDocument())
                return;
            Vector formOffset = preservedFormOffset ?? formScroll.Offset;
            Vector tableOffset = tableScroll.Offset;
            populateMemberList(viewModel.SelectedMemberId);
            restoreFormOffset(formOffset);
            tableScroll.Offset = tableOffset;
        });
    }

    private void populateMemberList(string? preferredMemberId = null, bool refreshContent = true)
    {
        IReadOnlyList<string> memberIds = viewModel.FilterMembers();
        string? nextSelection = viewModel.ChooseSelection(memberIds, preferredMemberId);
        syncingSelection = true;
        if (!memberIds.SequenceEqual(memberList.ItemsSource?.Cast<string>() ?? [], StringComparer.Ordinal))
            memberList.ItemsSource = memberIds;
        memberList.SelectedItem = nextSelection;
        syncingSelection = false;
        selectMember(nextSelection, refreshContent);
        if (viewModel.ViewMode == GeneralDataViewMode.Table)
            rebuildTable();
    }

    private void onMemberSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!syncingSelection)
            selectMember(memberList.SelectedItem as string);
    }

    private void selectMember(string? memberId, bool refreshContent = true)
    {
        bool selectionChanged = renderedMemberId != memberId;
        renderedMemberId = memberId;
        viewModel.SelectedMemberId = memberId;
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
        if (viewModel.ViewMode == GeneralDataViewMode.Form && (selectionChanged || refreshContent || memberId is null))
            buildForm(memberId);
    }

    private void updateViewMode()
    {
        bool showForm = viewModel.ViewMode == GeneralDataViewMode.Form;
        if (showForm)
            buildForm(selectedMemberId);
        else
            rebuildTable();
    }

    private void rebuildTable()
    {
        tableColumns = viewModel.GetColumns();
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

        GeneralDataTableRow[] rows = viewModel.GetRows();
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
        string type = GeneralDataParameterSchema.ReadTypeName(column.Definition["type"]);
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
                viewModel.UpdateMemberValue(row.Member, column.Name, JsonValue.Create(next));
            };
            return check;
        }
        if (type is "int" or "float")
            return buildTypedFieldEditor(column.Name, column.Definition, rawValue, row.Member, null);
        if (type == "string")
        {
            string current = rawValue?.GetValue<string>() ?? string.Empty;
            JsonObject? reference = GeneralDataParameterSchema.GetParamReference(column.Definition);
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
                    viewModel.UpdateMemberValue(row.Member, column.Name, JsonValue.Create(next));
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
                viewModel.UpdateMemberValue(row.Member, column.Name, JsonValue.Create(next));
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
        if (viewModel.GetMember(memberId) is JsonObject member && !viewModel.MatchesSearch(memberId, member))
        {
            viewModel.SearchText = string.Empty;
        }
        viewModel.SelectedMemberId = memberId;
        syncingViewMode = true;
        viewModel.ViewMode = GeneralDataViewMode.Form;
        syncingViewMode = false;
        populateMemberList(memberId);
    }

    private void revealMember(string memberId)
    {
        if (viewModel.GetMember(memberId) is JsonObject member && !viewModel.MatchesSearch(memberId, member))
        {
            viewModel.SearchText = string.Empty;
        }
        viewModel.SelectedMemberId = memberId;
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

        if (viewModel.CanEditAbilityGraph(memberId))
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

    private void openSelectedAbilityGraph()
    {
        if (viewModel.CanEditAbilityGraph(selectedMemberId))
            owner.showBlueprintEditor(typeKey, selectedMemberId!);
    }

    private async Task onAddMemberAsync()
    {
        string? id = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("NEW_MEMBER"),
            LocaleService.Get("ENTER_ID"),
            viewModel.MemberIds);
        if (string.IsNullOrWhiteSpace(id))
            return;
        if (!viewModel.CreateMember(id))
            return;
        revealMember(id);
    }

    private async Task onChangeMemberIdAsync(string oldId)
    {
        string? newId = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("CHANGE_ID"),
            LocaleService.Get("ENTER_ID"),
            viewModel.MemberIds.Where(k => k != oldId),
            oldId);
        if (string.IsNullOrWhiteSpace(newId) || newId == oldId)
            return;
        owner.closeBlueprintEditor(typeKey, oldId);
        if (!viewModel.RenameMember(oldId, newId))
            return;
        revealMember(newId);
    }

    private async Task onDuplicateMemberAsync(string sourceId)
    {
        if (!viewModel.ContainsMember(sourceId))
            return;
        string newId = viewModel.SuggestDuplicateId(sourceId);
        string? confirmedId = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("DUPLICATE_MEMBER"),
            LocaleService.Get("ENTER_ID"),
            viewModel.MemberIds,
            newId);
        if (string.IsNullOrWhiteSpace(confirmedId))
            return;
        if (!viewModel.DuplicateMember(sourceId, confirmedId))
            return;
        revealMember(confirmedId);
    }

    private void onRemoveMember(string memberId)
    {
        owner.closeBlueprintEditor(typeKey, memberId);
        if (!viewModel.DeleteMember(memberId))
            return;
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
        if (memberId is null || viewModel.GetMember(memberId) is not JsonObject member)
        {
            TextBlock placeholder = new()
            {
                Text = LocaleService.Get(viewModel.SearchText.Trim().Length == 0
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

        JsonObject? paramsObj = viewModel.ParameterDefinitions;

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
        JsonObject? reference = GeneralDataParameterSchema.GetParamReference(paramDef);
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

        if (!GeneralDataParameterSchema.IsParamReferenceAllowed(paramDef))
        {
            menu.Open(anchor);
            return;
        }

        menu.Items.Add(new Separator());
        MenuItem addReferenceItem = new() { Header = LocaleService.Get("ADD_REFERENCE") };
        foreach (string key in viewModel.ReferenceTypeKeys)
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

        if (GeneralDataParameterSchema.GetParamReference(paramDef) is not null)
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
        if (!viewModel.UpdateParameterReference(paramDef, reference))
            return;
        buildForm(selectedMemberId);
    }

    private static string formatReferenceLabel(JsonObject reference)
    {
        return reference["kind"]?.GetValue<string>() == "animation"
            ? LocaleService.Get("REFERENCE_TYPE_ANIMATION")
            : reference["key"]?.GetValue<string>() ?? string.Empty;
    }

    private async Task onAddParamAsync(string memberId)
    {
        GeneralDataParamCreation? result = await AddParamDialog.ShowAsync(
            owner, viewModel.ParameterNames);
        if (result is null)
            return;
        if (!viewModel.AddParameter(result))
            return;
        buildForm(memberId);
    }

    private async Task onEditParamAsync(string paramName)
    {
        GeneralDataParamCreation? initialValue = viewModel.GetParameter(paramName);
        if (initialValue is null)
            return;
        GeneralDataParamCreation? result = await AddParamDialog.ShowEditAsync(
            owner,
            viewModel.ParameterNames.Where(name => name != paramName),
            initialValue);
        if (result is null)
            return;

        bool resetMemberValues = GeneralDataParameterSchema.HasValueTypeChanged(initialValue, result);
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

        if (!viewModel.UpdateParameter(paramName, initialValue, result))
            return;
        populateMemberList(selectedMemberId);
    }

    private async Task onRemoveParamAsync(string paramName)
    {
        string msg = LocaleService.Get("CONFIRM_DELETE_PARAM").Replace("{}", paramName);
        bool confirmed = await ConfirmationDialog.ShowAsync(owner, LocaleService.Get("ADD_PARAM"), msg);
        if (!confirmed)
            return;
        if (!viewModel.DeleteParameter(paramName))
            return;
        buildForm(selectedMemberId);
    }

    private Control buildFieldEditor(string paramName, JsonObject paramDef, JsonNode? rawValue, JsonObject member)
    {
        string type = GeneralDataParameterSchema.ReadTypeName(paramDef["type"]);
        if (LuaMetadataType.Parse(type).Kind == LuaMetadataTypeKind.Union
            || type.StartsWith("Tuple[", StringComparison.Ordinal)
            || paramDef["type"] is JsonObject)
        {
            return buildTypedFieldEditor(paramName, paramDef, rawValue, member, null);
        }
        JsonObject? reference = GeneralDataParameterSchema.GetParamReference(paramDef);
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
                if (!viewModel.UpdateMemberValue(member, paramName, JsonValue.Create(next)))
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
                if (!viewModel.UpdateMemberValue(member, paramName, JsonValue.Create(assetPath)))
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
                    viewModel.UpdateMemberValue(member, paramName, tupleVal);
                };
                Grid.SetColumn(box, captured);
                tupleRow.Children.Add(box);
            }
            return tupleRow;
        }

        if (GeneralDataParameterSchema.IsSfType(type))
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
                    if (!viewModel.UpdateMemberValue(member, paramName, JsonValue.Create(next)))
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
                if (!viewModel.UpdateMemberValue(member, paramName, JsonValue.Create(next)))
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
        string type = GeneralDataParameterSchema.ReadTypeName(paramDef["type"]);
        string editorType = type switch
        {
            "list" => GeneralDataParameterSchema.GetContainerItemType(paramDef, "itemType") + "[]",
            "dict" => "Dict[string, " + GeneralDataParameterSchema.GetContainerItemType(paramDef, "valueType") + "]",
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
            CellSize = gameData.Configs.getCellSize(),
            HistoryGameData = gameData,
            ShowFieldNames = false,
            CustomValueEditorFactory = createGeneralDataReferenceEditor,
        };
        form.SetFields([field]);
        form.ValueChanged += (_, args) =>
        {
            if (JsonNode.DeepEquals(member[paramName], args.Value))
                return;
            viewModel.UpdateMemberValue(member, paramName, args.Value);
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
            if (GeneralDataParameterSchema.GetContainerItemType(paramDef, "itemType") == "file")
                itemMeta["PathVars"] = GameAssetPath.Root;
            if (referenceOptions is not null)
                itemMeta["GeneralDataReference"] = buildReferenceOptions(referenceOptions);
            if (itemMeta.Count > 0)
                meta["ItemMeta"] = itemMeta;
        }
        else if (type == "dict")
        {
            if (GeneralDataParameterSchema.GetContainerItemType(paramDef, "valueType") == "file")
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

    private List<string>? getRefOptions(string refKind, string refKey)
    {
        if (refKind == "animation")
            return gameData.Assets.AnimationsData.Keys.OrderBy(k => k, StringComparer.Ordinal).ToList();
        if (refKind == "general" && !string.IsNullOrEmpty(refKey))
            return viewModel.GetReferenceMemberIds(refKey);
        return null;
    }

}
