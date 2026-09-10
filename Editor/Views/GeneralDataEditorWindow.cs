using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class GeneralDataEditorWindow : Window, IProjectSaveParticipant
{
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintPreviewService previewService;
    private readonly Toast toast;
    private readonly EditorDocumentBinding documentBinding;
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
        FontFamily = FontFamily.Parse("avares://Ludork/Editor/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        tabControl = new TabControl
        {
            Background = new SolidColorBrush(Color.FromRgb(30, 30, 30)),
        };
        tabControl.AddHandler(PointerPressedEvent, onTabPointerPressed, RoutingStrategies.Bubble);
        tabControl.SelectionChanged += (_, _) =>
        {
            if (!buildingTabs)
            {
                ensureSelectedPage();
                documentBinding?.Refresh();
            }
        };

        Content = DeferredWindowInitializer.CreateLoadingContent();
        HistoryMergeBehavior.AttachBoundary(this, gameData);
        toast = new Toast(this);
        documentBinding = new EditorDocumentBinding(this, gameData,
            () => (tabControl.SelectedItem as TabItem)?.Tag is string key ? gameData.GetDocument("General", key) : null,
            () => LocaleService.Get("GENERAL_DATA_EDITOR"));
        projectSave.RegisterParticipant(this);
        initializer = new DeferredWindowInitializer(this, () =>
        {
            Content = tabControl;
            buildTabs(pendingTypeKey);
            pendingTypeKey = null;
        });


        gameData.DataReloaded += onDataReloaded;
        gameData.Documents.Changed += onDocumentsChanged;
        Closed += (_, _) =>
        {
            projectSave.UnregisterParticipant(this);

            gameData.DataReloaded -= onDataReloaded;
            gameData.Documents.Changed -= onDocumentsChanged;
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
                Header = new DocumentStatusPresenter(gameData, "General", entry.Key),
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
        documentBinding.Refresh();
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

    private void onDocumentsChanged(object? sender, EventArgs args)
    {
        BlueprintEditorWindow[] windows = blueprintWindows.Values.Distinct().ToArray();
        blueprintWindows.Clear();
        foreach (BlueprintEditorWindow window in windows)
            blueprintWindows[window.Document.DocumentKey] = window;
        if (!initializer.IsInitialized)
            return;
        string[] keys = gameData.GeneralData.Keys.OrderBy(key => key, StringComparer.Ordinal).ToArray();
        if (!keys.SequenceEqual(tabControl.Items.OfType<TabItem>().Select(tab => tab.Tag as string), StringComparer.Ordinal))
            buildTabs(documentBinding.Document?.Key);
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

    public void FlushPendingChanges() => FlushBlueprintEditors();

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
            document.Dispose();
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
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
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
        if (!gameData.AddGeneralEvent(typeKey, eventName!))
            return;
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
        if (!gameData.RenameGeneralEvent(typeKey, currentName, newName!))
            return;
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
        if (!gameData.DeleteGeneralEvent(typeKey, eventName))
            return;
        buildTabs(typeKey);
    }

    private async Task onDeleteDataTypeAsync(string typeKey)
    {
        string msg = LocaleService.Get("CONFIRM_DELETE_DATA_TYPE").Replace("{}", typeKey) + Environment.NewLine + LocaleService.Get("DELETE_DOCUMENT_CONFIRMATION");
        bool confirmed = await ConfirmationDialog.ShowAsync(this, LocaleService.Get("DELETE_DATA_TYPE"), msg);
        if (!confirmed)
            return;
        if (!await EditorResourceOperations.DeleteAsync(this, () => gameData.DeleteGeneralType(typeKey)))
            return;
        closeBlueprintEditors(typeKey);
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
}

internal enum GeneralDataViewMode
{
    Form,
    Table,
}

internal sealed record GeneralDataTableColumn(string Name, JsonObject Definition);

internal sealed record GeneralDataTableRow(string Id, JsonObject Member);
