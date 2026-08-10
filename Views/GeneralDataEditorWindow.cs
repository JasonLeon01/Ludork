using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
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
    private readonly Dictionary<string, BlueprintEditorWindow> blueprintWindows = new(StringComparer.Ordinal);

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

        Content = tabControl;
        toast = new Toast(this);
        buildTabs(null);

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
        string? selectedKey = (tabControl.SelectedItem as TabItem)?.Tag as string;
        buildTabs(selectedKey);
    }

    private void buildTabs(string? preserveKey)
    {
        tabControl.Items.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in gameData.GeneralData.OrderBy(e => e.Key, StringComparer.Ordinal))
        {
            TabItem tab = new()
            {
                Header = entry.Key,
                Tag = entry.Key,
                Content = new GeneralDataPage(this, gameData, entry.Key, entry.Value),
            };
            tabControl.Items.Add(tab);
            if (string.Equals(entry.Key, preserveKey, StringComparison.Ordinal))
                tabControl.SelectedItem = tab;
        }
        if (tabControl.SelectedItem is null && tabControl.Items.Count > 0)
            tabControl.SelectedItem = tabControl.Items[0];
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        string? selectedKey = (tabControl.SelectedItem as TabItem)?.Tag as string;
        buildTabs(selectedKey);
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
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
            MenuItem linkItem = new() { Header = LocaleService.Get("SET_LINKED_TYPE") };
            linkItem.Click += async (_, _) => await onSetLinkedTypeAsync(typeKey);
            menu.Items.Add(linkItem);
            menu.Items.Add(new Separator());
            MenuItem deleteItem = new() { Header = LocaleService.Get("DELETE_DATA_TYPE") };
            deleteItem.Click += async (_, _) => await onDeleteDataTypeAsync(typeKey);
            menu.Items.Add(deleteItem);
        }
        menu.Open(this);
    }

    private async Task onAddDataTypeAsync()
    {
        List<string> infoTypes = getInfoTypes();
        string noLink = LocaleService.Get("NO_LINKED_TYPE");
        string? linkedType = await ItemSelectorDialog.ShowAsync(
            this,
            LocaleService.Get("SELECT_LINKED_TYPE"),
            LocaleService.Get("SELECT_LINKED_TYPE_DESC"),
            new[] { noLink }.Concat(infoTypes));
        if (linkedType is null)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_DATA_TYPE"),
            LocaleService.Get("ENTER_DATA_TYPE_NAME"),
            gameData.GeneralData.Keys);
        if (string.IsNullOrWhiteSpace(name))
            return;
        string? actualLinked = linkedType == noLink ? null : linkedType;
        gameData.CreateGeneralType(name, actualLinked);
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
        buildTabs(newName);
    }

    private async Task onSetLinkedTypeAsync(string typeKey)
    {
        List<string> infoTypes = getInfoTypes();
        string noLink = LocaleService.Get("NO_LINKED_TYPE");
        JsonObject typeData = gameData.GeneralData[typeKey];
        string? currentLinked = typeData["linkedType"]?.GetValue<string>();
        string initialSelection = currentLinked is not null && infoTypes.Contains(currentLinked)
            ? currentLinked : noLink;

        string? chosen = await ItemSelectorDialog.ShowAsync(
            this,
            LocaleService.Get("SELECT_LINKED_TYPE"),
            LocaleService.Get("SELECT_LINKED_TYPE_DESC"),
            new[] { noLink }.Concat(infoTypes),
            initialSelection);
        if (chosen is null)
            return;
        string? selectedLinked = chosen == noLink ? null : chosen;
        if (string.Equals(selectedLinked, currentLinked, StringComparison.Ordinal))
            return;
        closeBlueprintEditors(typeKey);
        gameData.RecordSnapshot();
        if (selectedLinked is null)
            typeData.Remove("linkedType");
        else
            typeData["linkedType"] = selectedLinked;
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
        buildTabs(null);
    }

    private List<string> getInfoTypes()
    {
        return metadataService.EnumerateTypes()
            .Where(metadata => metadata.Type.ModuleName?.StartsWith("Source.Infos.", StringComparison.Ordinal) == true)
            .Where(metadata => classResolver.IsDerivedFrom(metadata.Type.QualifiedName, "Engine.InfoBase"))
            .Select(metadata => metadata.Type.TypeName)
            .Distinct(StringComparer.Ordinal)
            .OrderBy(typeName => typeName, StringComparer.Ordinal)
            .ToList();
    }
}

internal sealed class GeneralDataPage : Grid
{
    private readonly GeneralDataEditorWindow owner;
    private readonly GameDataService gameData;
    private readonly string typeKey;
    private readonly JsonObject typeData;
    private readonly ListBox memberList;
    private readonly Border linkedTypeBar;
    private readonly TextBlock linkedTypeLabel;
    private readonly Button editBlueprintButton;
    private readonly ScrollViewer formScroll;
    private readonly StackPanel formContent;

    private string? selectedMemberId;

    public GeneralDataPage(
        GeneralDataEditorWindow owner,
        GameDataService gameData,
        string typeKey,
        JsonObject typeData)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.typeKey = typeKey;
        this.typeData = typeData;

        ColumnDefinitions = new ColumnDefinitions("240,4,*");

        Grid leftGrid = new() { RowDefinitions = new RowDefinitions("*,Auto") };
        memberList = new ListBox
        {
            Background = new SolidColorBrush(Color.FromRgb(40, 40, 40)),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        memberList.SelectionChanged += onMemberSelectionChanged;
        memberList.AddHandler(PointerPressedEvent, onMemberListPointerPressed, RoutingStrategies.Bubble);
        Grid.SetRow(memberList, 0);
        leftGrid.Children.Add(memberList);

        Button addMemberBtn = new()
        {
            Content = "+ " + LocaleService.Get("NEW_MEMBER"),
            Margin = new Thickness(4),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        addMemberBtn.Click += async (_, _) => await onAddMemberAsync();
        Grid.SetRow(addMemberBtn, 1);
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
        linkedTypeBar = new Border
        {
            Background = new SolidColorBrush(Color.FromRgb(45, 45, 45)),
            Padding = new Thickness(8, 6),
            IsVisible = false,
        };
        linkedTypeLabel = new TextBlock
        {
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = new SolidColorBrush(Color.Parse("#88aaff")),
            FontWeight = FontWeight.Bold,
        };
        editBlueprintButton = new Button
        {
            Content = LocaleService.Get("EDIT_BLUEPRINT"),
            IsEnabled = false,
        };
        editBlueprintButton.Click += (_, _) => openSelectedBlueprint();
        Grid linkedTypeContent = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
        };
        linkedTypeContent.Children.Add(linkedTypeLabel);
        Grid.SetColumn(editBlueprintButton, 1);
        linkedTypeContent.Children.Add(editBlueprintButton);
        linkedTypeBar.Child = linkedTypeContent;
        Grid.SetRow(linkedTypeBar, 0);
        rightGrid.Children.Add(linkedTypeBar);

        formContent = new StackPanel { Spacing = 6, Margin = new Thickness(8) };
        formScroll = new ScrollViewer
        {
            Content = formContent,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled,
        };
        Grid.SetRow(formScroll, 1);
        rightGrid.Children.Add(formScroll);

        Grid.SetColumn(rightGrid, 2);
        Children.Add(rightGrid);

        populateMemberList();
        updateLinkedTypeBar();
    }

    private void populateMemberList()
    {
        memberList.Items.Clear();
        if (typeData["members"] is JsonObject members)
        {
            foreach (string id in members.Select(e => e.Key).OrderBy(k => k, StringComparer.Ordinal))
                memberList.Items.Add(id);
        }
        if (memberList.Items.Count > 0)
            memberList.SelectedItem = memberList.Items[0];
        else
            buildForm(null);
    }

    private void updateLinkedTypeBar()
    {
        string? linked = typeData["linkedType"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(linked))
        {
            linkedTypeBar.IsVisible = false;
        }
        else
        {
            linkedTypeLabel.Text = LocaleService.Get("LINKED_TYPE") + ": " + linked;
            linkedTypeBar.IsVisible = true;
        }
        updateEditBlueprintButton();
    }

    private void onMemberSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        selectedMemberId = memberList.SelectedItem as string;
        buildForm(selectedMemberId);
        updateEditBlueprintButton();
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
        showMemberContextMenu(hitId);
    }

    private void showMemberContextMenu(string? memberId)
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

            if (canEditBlueprint(memberId))
            {
                MenuItem editBlueprintItem = new() { Header = LocaleService.Get("EDIT_BLUEPRINT") };
                editBlueprintItem.Click += (_, _) => owner.showBlueprintEditor(typeKey, memberId);
                menu.Items.Add(editBlueprintItem);
            }

            menu.Items.Add(new Separator());

            MenuItem removeItem = new() { Header = LocaleService.Get("REMOVE_MEMBER") };
            removeItem.Click += (_, _) => onRemoveMember(memberId);
            menu.Items.Add(removeItem);
        }
        menu.Open(memberList);
    }

    private void updateEditBlueprintButton()
    {
        editBlueprintButton.IsEnabled = canEditBlueprint(selectedMemberId);
    }

    private bool canEditBlueprint(string? memberId)
    {
        if (string.IsNullOrWhiteSpace(memberId)
            || string.IsNullOrWhiteSpace(typeData["linkedType"]?.GetValue<string>())
            || typeData["members"]?[memberId] is not JsonObject
            || typeData["events"] is not JsonArray events)
        {
            return false;
        }
        return events.Any(value => value is JsonValue scalar
            && scalar.TryGetValue(out string? eventName)
            && !string.IsNullOrWhiteSpace(eventName));
    }

    private void openSelectedBlueprint()
    {
        if (canEditBlueprint(selectedMemberId))
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
        populateMemberList();
        memberList.SelectedItem = id;
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
        populateMemberList();
        memberList.SelectedItem = newId;
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
        populateMemberList();
        memberList.SelectedItem = confirmedId;
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
                Text = LocaleService.Get("GENERAL_DATA_NO_MEMBERS"),
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
        string description = paramDef?["desc"]?.GetValue<string>() ?? string.Empty;
        if (description.Length > 0 || referenceLabel.Length > 0)
        {
            ToolTip.SetTip(
                labelBlock,
                description.Length > 0 && referenceLabel.Length > 0
                    ? description + Environment.NewLine + referenceLabel
                    : description + referenceLabel);
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
        if (!isParamReferenceAllowed(paramDef))
            return;

        ContextMenu menu = new();
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
        return type is "string" or "list" or "dict";
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
        (string name, string type, string defaultText)? result = await AddParamDialog.ShowAsync(
            owner, paramsObj.Select(e => e.Key));
        if (result is null)
            return;
        (string name, string type, string defaultText) = result.Value;
        JsonNode defaultValue = parseDefaultValue(type, defaultText);
        gameData.RecordSnapshot();
        paramsObj[name] = new JsonObject { ["type"] = type, ["defaultValue"] = defaultValue };
        typeData["params"] = paramsObj;
        if (typeData["members"] is JsonObject members)
        {
            foreach (KeyValuePair<string, JsonNode?> mEntry in members)
            {
                if (mEntry.Value is JsonObject member && !member.ContainsKey(name))
                    member[name] = defaultValue?.DeepClone() ?? JsonValue.Create("");
            }
        }
        gameData.refreshModifiedState();
        buildForm(memberId);
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
                if ((rawValue?.GetValue<bool?>() ?? false) == next)
                    return;
                gameData.RecordSnapshot();
                member[paramName] = next;
                gameData.refreshModifiedState();
            };
            return check;
        }

        if (type == "int")
        {
            int current = rawValue?.GetValue<int?>() ?? 0;
            NumericUpDown num = EditorInputs.CreateNumericUpDown(current, -999999, 999999, 1);
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
                string defaultSub = paramDef["defaultValue"]?.GetValue<string>() ?? string.Empty;
                string assetsRoot = Path.Combine(gameData.ProjectPath, "Assets");
                string startDir = !string.IsNullOrWhiteSpace(defaultSub)
                    ? Path.Combine(assetsRoot, defaultSub)
                    : assetsRoot;
                if (!Directory.Exists(startDir))
                    startDir = assetsRoot;
                string? path = await FileSelectorDialog.ShowAsync(owner, startDir, FileSelectorDialog.AllFilesFilter());
                if (path is null)
                    return;
                string rel = Path.GetRelativePath(assetsRoot, path).Replace('\\', '/');
                gameData.RecordSnapshot();
                member[paramName] = rel;
                rawValue = member[paramName];
                gameData.refreshModifiedState();
                pathBox.Text = rel;
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
            return new ListFieldEditor(gameData, member, paramName, current, refOptions);
        }

        if (type == "dict")
        {
            JsonObject current = rawValue is JsonObject obj ? (JsonObject)obj.DeepClone() : new JsonObject();
            List<string>? refOptions = getRefOptions(refKind, refKey);
            return new DictFieldEditor(gameData, member, paramName, current, refOptions);
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

        {
            string current = rawValue?.GetValue<string>() ?? string.Empty;
            List<string>? refOptions = getRefOptions(refKind, refKey);
            if (refOptions is not null)
            {
                ComboBox combo = new()
                {
                    MinHeight = EditorInputs.FieldMinHeight,
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                };
                List<string> items = new() { string.Empty };
                if (current.Length > 0 && !refOptions.Contains(current))
                    items.Add(current);
                items.AddRange(refOptions);
                foreach (string opt in items)
                    combo.Items.Add(opt);
                combo.SelectedItem = items.Contains(current) ? current : string.Empty;
                combo.SelectionChanged += (_, _) =>
                {
                    string next = combo.SelectedItem as string ?? string.Empty;
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
                string type = paramDef["type"]?.GetValue<string>() ?? "string";
                JsonNode? def = paramDef["defaultValue"];
                member[entry.Key] = buildMemberDefaultValue(type, def);
            }
        }
        return member;
    }

    private static JsonNode? buildMemberDefaultValue(string type, JsonNode? defaultDef)
    {
        return type switch
        {
            "int" => defaultDef?.GetValue<int?>() ?? 0,
            "float" => defaultDef?.GetValue<double?>() ?? 0.0,
            "bool" => defaultDef?.GetValue<bool?>() ?? false,
            "list" => defaultDef is JsonArray arr ? (JsonArray)arr.DeepClone() : new JsonArray(),
            "dict" => defaultDef is JsonObject obj ? (JsonObject)obj.DeepClone() : new JsonObject(),
            _ => JsonValue.Create(defaultDef?.GetValue<string>() ?? string.Empty),
        };
    }

    private static void reorderMemberKey(JsonObject members, string oldKey, string newKey)
    {
        List<KeyValuePair<string, JsonNode?>> entries = members.Select(e => e).ToList();
        members.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            members.Add(entry.Key == oldKey ? newKey : entry.Key, entry.Value);
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
            _ => JsonValue.Create(text) ?? JsonValue.Create(string.Empty)!,
        };
    }
}

internal sealed class ListFieldEditor : StackPanel
{
    private readonly GameDataService gameData;
    private readonly JsonObject member;
    private readonly string paramName;
    private readonly JsonArray data;
    private readonly List<string>? refOptions;

    public ListFieldEditor(GameDataService gameData, JsonObject member, string paramName, JsonArray data, List<string>? refOptions)
    {
        this.gameData = gameData;
        this.member = member;
        this.paramName = paramName;
        this.data = data;
        this.refOptions = refOptions;
        Spacing = 4;
        member[paramName] = data;
        rebuild();
    }

    private void rebuild()
    {
        Children.Clear();
        for (int i = 0; i < data.Count; i++)
        {
            int captured = i;
            string current = data[i]?.GetValue<string>() ?? string.Empty;
            Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 4 };
            Control editor;
            if (refOptions is not null)
            {
                ComboBox combo = new()
                {
                    MinHeight = EditorInputs.FieldMinHeight,
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                };
                List<string> items = new() { string.Empty };
                if (current.Length > 0 && !refOptions.Contains(current))
                    items.Add(current);
                items.AddRange(refOptions);
                foreach (string opt in items)
                    combo.Items.Add(opt);
                combo.SelectedItem = items.Contains(current) ? current : string.Empty;
                combo.SelectionChanged += (_, _) =>
                {
                    string next = combo.SelectedItem as string ?? string.Empty;
                    if ((data[captured]?.GetValue<string>() ?? string.Empty) == next)
                        return;
                    gameData.RecordSnapshot();
                    data[captured] = next;
                    member[paramName] = data;
                    gameData.refreshModifiedState();
                };
                editor = combo;
            }
            else
            {
                TextBox box = EditorInputs.CreateEditableTextBox(current);
                box.TextChanged += (_, _) =>
                {
                    string next = box.Text ?? string.Empty;
                    if ((data[captured]?.GetValue<string>() ?? string.Empty) == next)
                        return;
                    gameData.RecordSnapshot();
                    data[captured] = next;
                    member[paramName] = data;
                    gameData.refreshModifiedState();
                };
                editor = box;
            }
            row.Children.Add(editor);
            Button del = new() { Content = "−", Width = 28, Height = 34, Padding = new Thickness(0) };
            del.Click += (_, _) =>
            {
                gameData.RecordSnapshot();
                data.RemoveAt(captured);
                member[paramName] = data;
                gameData.refreshModifiedState();
                rebuild();
            };
            Grid.SetColumn(del, 1);
            row.Children.Add(del);
            Children.Add(row);
        }
        Button add = new()
        {
            Content = "+",
            HorizontalAlignment = HorizontalAlignment.Stretch,
            Height = 28,
        };
        add.Click += (_, _) =>
        {
            gameData.RecordSnapshot();
            data.Add(string.Empty);
            member[paramName] = data;
            gameData.refreshModifiedState();
            rebuild();
        };
        Children.Add(add);
    }
}

internal sealed class DictFieldEditor : StackPanel
{
    private readonly GameDataService gameData;
    private readonly JsonObject member;
    private readonly string paramName;
    private JsonObject data;
    private readonly List<string>? keyOptions;

    public DictFieldEditor(GameDataService gameData, JsonObject member, string paramName, JsonObject data, List<string>? keyOptions)
    {
        this.gameData = gameData;
        this.member = member;
        this.paramName = paramName;
        this.data = data;
        this.keyOptions = keyOptions;
        Spacing = 4;
        member[paramName] = data;
        rebuild();
    }

    private void rebuild()
    {
        Children.Clear();
        List<KeyValuePair<string, JsonNode?>> entries = data.Select(e => e).ToList();
        for (int i = 0; i < entries.Count; i++)
        {
            string entryKey = entries[i].Key;
            string entryValue = entries[i].Value?.GetValue<string>() ?? string.Empty;
            Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,4,*,Auto"), ColumnSpacing = 4 };
            Control keyEditor;
            if (keyOptions is not null)
            {
                ComboBox combo = new()
                {
                    MinHeight = EditorInputs.FieldMinHeight,
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                };
                List<string> items = new() { string.Empty };
                if (entryKey.Length > 0 && !keyOptions.Contains(entryKey))
                    items.Add(entryKey);
                items.AddRange(keyOptions);
                foreach (string opt in items)
                    combo.Items.Add(opt);
                combo.SelectedItem = items.Contains(entryKey) ? entryKey : string.Empty;
                string capturedKey = entryKey;
                combo.SelectionChanged += (_, _) =>
                {
                    string newKey = combo.SelectedItem as string ?? string.Empty;
                    if (newKey == capturedKey || string.IsNullOrEmpty(newKey))
                        return;
                    gameData.RecordSnapshot();
                    string val = data[capturedKey]?.GetValue<string>() ?? string.Empty;
                    data.Remove(capturedKey);
                    data[newKey] = val;
                    member[paramName] = data;
                    gameData.refreshModifiedState();
                    rebuild();
                };
                keyEditor = combo;
            }
            else
            {
                TextBox keyBox = EditorInputs.CreateEditableTextBox(entryKey);
                string capturedKey = entryKey;
                keyBox.LostFocus += (_, _) =>
                {
                    string newKey = keyBox.Text?.Trim() ?? string.Empty;
                    if (string.IsNullOrEmpty(newKey) || newKey == capturedKey || data.ContainsKey(newKey))
                        return;
                    gameData.RecordSnapshot();
                    string val = data[capturedKey]?.GetValue<string>() ?? string.Empty;
                    data.Remove(capturedKey);
                    data[newKey] = val;
                    member[paramName] = data;
                    gameData.refreshModifiedState();
                    rebuild();
                };
                keyEditor = keyBox;
            }
            row.Children.Add(keyEditor);

            TextBox valBox = EditorInputs.CreateEditableTextBox(entryValue);
            string capturedEntryKey = entryKey;
            valBox.TextChanged += (_, _) =>
            {
                if (!data.ContainsKey(capturedEntryKey))
                    return;
                string next = valBox.Text ?? string.Empty;
                if ((data[capturedEntryKey]?.GetValue<string>() ?? string.Empty) == next)
                    return;
                gameData.RecordSnapshot();
                data[capturedEntryKey] = next;
                member[paramName] = data;
                gameData.refreshModifiedState();
            };
            Grid.SetColumn(valBox, 2);
            row.Children.Add(valBox);

            Button del = new() { Content = "−", Width = 28, Height = 34, Padding = new Thickness(0) };
            string capturedDelKey = entryKey;
            del.Click += (_, _) =>
            {
                gameData.RecordSnapshot();
                data.Remove(capturedDelKey);
                member[paramName] = data;
                gameData.refreshModifiedState();
                rebuild();
            };
            Grid.SetColumn(del, 3);
            row.Children.Add(del);
            Children.Add(row);
        }
        Button add = new()
        {
            Content = "+",
            HorizontalAlignment = HorizontalAlignment.Stretch,
            Height = 28,
        };
        add.Click += (_, _) =>
        {
            string newKey = "key";
            int n = 2;
            while (data.ContainsKey(newKey))
                newKey = "key" + n++;
            gameData.RecordSnapshot();
            data[newKey] = string.Empty;
            member[paramName] = data;
            gameData.refreshModifiedState();
            rebuild();
        };
        Children.Add(add);
    }
}
