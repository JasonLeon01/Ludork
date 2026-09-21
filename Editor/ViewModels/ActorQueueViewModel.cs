using Avalonia.Media;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace Ludork.ViewModels;

public enum ActorLibraryScope
{
    All,
    Favourites,
    Recent,
}

public sealed record ActorLibraryScopeOption(ActorLibraryScope Scope, string DisplayName);

public sealed partial class ActorQueueViewModel : ViewModelBase, IDisposable
{
    private const int MaximumRecentItems = 20;
    private const string BlueprintPrefix = "Data.Blueprints.";
    private readonly ProjectDataStore gameData;
    private readonly ProjectConfigService projectConfig;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintPreviewService previewService;
    private readonly IImage placeholder;
    private readonly Dictionary<string, ActorQueueItemViewModel> catalog = new(StringComparer.Ordinal);
    private readonly List<string> recentReferences = [];
    private bool refreshing;
    private bool deferVisibleRefresh;
    private bool disposed;
    private long visualRevision;
    private string? selectedReference;
    [ObservableProperty] private ActorQueueItemViewModel? selectedItem;
    [ObservableProperty] private string searchText = string.Empty;
    [ObservableProperty] private ActorLibraryScopeOption? selectedScope;
    [ObservableProperty] private string? selectedCategory;

    public ActorQueueViewModel(
        ProjectDataStore gameData,
        ProjectConfigService projectConfig,
        BlueprintClassResolver classResolver,
        BlueprintPreviewService previewService)
    {
        this.gameData = gameData;
        this.projectConfig = projectConfig;
        this.classResolver = classResolver;
        this.previewService = previewService;
        placeholder = EditorIconResources.GetImage("EditorImage.File");
        Scopes.Add(new ActorLibraryScopeOption(ActorLibraryScope.All, LocaleService.Get("ACTOR_LIBRARY_ALL")));
        Scopes.Add(new ActorLibraryScopeOption(ActorLibraryScope.Favourites, LocaleService.Get("ACTOR_LIBRARY_FAVOURITES")));
        Scopes.Add(new ActorLibraryScopeOption(ActorLibraryScope.Recent, LocaleService.Get("ACTOR_LIBRARY_RECENT")));
        SelectedScope = Scopes[0];
        gameData.DataReloaded += onDataReloaded;
        previewService.LiveVisualsInvalidated += onDataReloaded;
        refreshCatalog();
    }

    public ObservableCollection<ActorQueueItemViewModel> Items { get; } = [];
    public bool IsReadOnly { get; set; }
    public ObservableCollection<ActorLibraryScopeOption> Scopes { get; } = [];
    public ObservableCollection<string> Categories { get; } = [];
    public IReadOnlyList<string> BlueprintReferences => catalog.Keys.ToArray();
    public event EventHandler<string?>? SelectionChanged;
    public event EventHandler<string>? BlueprintOpenRequested;
    public event EventHandler<string>? BlueprintLocateRequested;

    partial void OnSelectedItemChanged(ActorQueueItemViewModel? value)
    {
        if (refreshing)
            return;
        selectedReference = value?.BlueprintReference;
        if (value is not null)
        {
            promoteRecent(value.BlueprintReference);
            if (SelectedScope?.Scope == ActorLibraryScope.Recent)
                refreshVisibleItems();
        }
        SelectionChanged?.Invoke(this, selectedReference);
    }

    partial void OnSearchTextChanged(string value)
    {
        requestVisibleItemsRefresh();
    }

    partial void OnSelectedScopeChanged(ActorLibraryScopeOption? value)
    {
        requestVisibleItemsRefresh();
    }

    partial void OnSelectedCategoryChanged(string? value)
    {
        requestVisibleItemsRefresh();
    }

    public void Select(ActorQueueItemViewModel? item, bool notifyIfUnchanged = false)
    {
        if (SelectedItem != item)
        {
            SelectedItem = item;
            return;
        }
        if (notifyIfUnchanged)
            SelectionChanged?.Invoke(this, item?.BlueprintReference);
    }

    public void AddOrPromote(string blueprintReference)
    {
        ActorQueueItemViewModel? item = null;
        deferVisibleRefresh = true;
        try
        {
            if (!catalog.TryGetValue(blueprintReference, out item))
            {
                refreshCatalog();
                catalog.TryGetValue(blueprintReference, out item);
            }
            if (item is not null)
            {
                promoteRecent(blueprintReference);
                selectedReference = blueprintReference;
                SearchText = string.Empty;
                SelectedCategory = Categories.FirstOrDefault();
                SelectedScope = Scopes.First(option => option.Scope == ActorLibraryScope.Recent);
            }
        }
        finally
        {
            deferVisibleRefresh = false;
        }
        refreshVisibleItems();
        if (item is null)
            return;
        Select(item, true);
    }

    public void Remove(ActorQueueItemViewModel item, ActorQueueItemViewModel? previousItem = null)
    {
        RemoveRecent(item, previousItem);
    }

    public void RemoveRecent(ActorQueueItemViewModel item, ActorQueueItemViewModel? previousItem = null)
    {
        recentReferences.Remove(item.BlueprintReference);
        item.IsRecent = false;
        if (selectedReference == item.BlueprintReference)
        {
            selectedReference = previousItem?.BlueprintReference;
            SelectionChanged?.Invoke(this, selectedReference);
        }
        refreshVisibleItems();
    }

    public void ToggleFavorite(ActorQueueItemViewModel item)
    {
        if (IsReadOnly)
            return;
        item.IsFavorite = !item.IsFavorite;
        projectConfig.SetActorFavorite(item.BlueprintReference, item.IsFavorite);
        if (SelectedScope?.Scope == ActorLibraryScope.Favourites)
            refreshVisibleItems();
    }

    public void RemapReferences(IReadOnlyDictionary<string, string> references)
    {
        foreach (KeyValuePair<string, string> pair in references)
            projectConfig.RemapActorFavorite(pair.Key, pair.Value);
        for (int index = 0; index < recentReferences.Count; index++)
        {
            if (references.TryGetValue(recentReferences[index], out string? replacement))
                recentReferences[index] = replacement;
        }
        if (selectedReference is not null
            && references.TryGetValue(selectedReference, out string? selectedReplacement))
        {
            selectedReference = selectedReplacement;
            SelectionChanged?.Invoke(this, selectedReference);
        }
    }

    public void PurgeStale()
    {
        refreshCatalog();
    }

    public void RequestOpen(ActorQueueItemViewModel? item)
    {
        if (!IsReadOnly && item is not null)
            BlueprintOpenRequested?.Invoke(this, item.BlueprintReference);
    }

    public void RequestLocate(ActorQueueItemViewModel? item)
    {
        if (item is not null)
            BlueprintLocateRequested?.Invoke(this, item.BlueprintReference);
    }

    public void DeactivatePreviews()
    {
        foreach (ActorQueueItemViewModel item in catalog.Values)
            item.DeactivatePreviews();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.DataReloaded -= onDataReloaded;
        previewService.LiveVisualsInvalidated -= onDataReloaded;
        foreach (ActorQueueItemViewModel item in catalog.Values)
            item.Dispose();
        catalog.Clear();
        Items.Clear();
        SelectedItem = null;
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        visualRevision++;
        refreshCatalog();
    }

    private void promoteRecent(string blueprintReference)
    {
        recentReferences.Remove(blueprintReference);
        recentReferences.Insert(0, blueprintReference);
        if (recentReferences.Count > MaximumRecentItems)
            recentReferences.RemoveRange(MaximumRecentItems, recentReferences.Count - MaximumRecentItems);
        foreach (ActorQueueItemViewModel catalogItem in catalog.Values)
            catalogItem.IsRecent = recentReferences.Contains(catalogItem.BlueprintReference, StringComparer.Ordinal);
    }

    private void refreshCatalog()
    {
        if (disposed)
            return;
        using IDisposable resolutionBatch = classResolver.BeginBatch();
        bool selectedRemoved = false;
        HashSet<string> validReferences = new HashSet<string>(StringComparer.Ordinal);
        foreach (string key in gameData.Blueprints.BlueprintsData.Keys)
        {
            string reference = BlueprintPrefix + key.Replace('/', '.');
            ResolvedBlueprintClass resolved = classResolver.Resolve(reference);
            if (!classResolver.IsDerivedFrom(resolved, "Engine.Actor"))
                continue;
            validReferences.Add(reference);
            if (catalog.TryGetValue(reference, out ActorQueueItemViewModel? existing))
            {
                existing.UpdateSource(resolved, visualRevision);
                existing.IsFavorite = projectConfig.IsActorFavorite(reference);
                existing.IsRecent = recentReferences.Contains(reference, StringComparer.Ordinal);
                continue;
            }
            catalog[reference] = new ActorQueueItemViewModel(
                reference,
                placeholder,
                resolved,
                previewService,
                visualRevision,
                projectConfig.IsActorFavorite(reference),
                recentReferences.Contains(reference, StringComparer.Ordinal));
        }
        foreach (string staleFavorite in projectConfig.GetActorFavorites()
            .Where(reference => !validReferences.Contains(reference)))
        {
            projectConfig.SetActorFavorite(staleFavorite, false);
        }
        foreach (string staleReference in catalog.Keys.Where(reference => !validReferences.Contains(reference)).ToArray())
        {
            projectConfig.SetActorFavorite(staleReference, false);
            catalog[staleReference].Dispose();
            catalog.Remove(staleReference);
            recentReferences.Remove(staleReference);
            if (selectedReference == staleReference)
            {
                selectedReference = null;
                selectedRemoved = true;
            }
        }
        string[] categories = catalog.Values
            .Select(item => item.Category)
            .Distinct(StringComparer.Ordinal)
            .OrderBy(value => value, StringComparer.Ordinal)
            .Prepend(LocaleService.Get("ALL_CATEGORIES"))
            .ToArray();
        if (!Categories.SequenceEqual(categories, StringComparer.Ordinal))
        {
            Categories.Clear();
            foreach (string category in categories)
                Categories.Add(category);
        }
        if (SelectedCategory is null || !Categories.Contains(SelectedCategory))
        {
            bool previousDeferVisibleRefresh = deferVisibleRefresh;
            deferVisibleRefresh = true;
            SelectedCategory = Categories.FirstOrDefault();
            deferVisibleRefresh = previousDeferVisibleRefresh;
        }
        requestVisibleItemsRefresh();
        if (selectedRemoved)
            SelectionChanged?.Invoke(this, null);
    }

    private void refreshVisibleItems()
    {
        if (disposed)
            return;
        ActorLibraryScope scope = SelectedScope?.Scope ?? ActorLibraryScope.All;
        string allCategories = Categories.FirstOrDefault() ?? LocaleService.Get("ALL_CATEGORIES");
        string category = SelectedCategory ?? allCategories;
        string query = SearchText.Trim();
        IEnumerable<ActorQueueItemViewModel> source = scope == ActorLibraryScope.Recent
            ? recentReferences
                .Select(reference => catalog.GetValueOrDefault(reference))
                .OfType<ActorQueueItemViewModel>()
            : catalog.Values.OrderBy(item => item.Category, StringComparer.Ordinal)
                .ThenBy(item => item.DisplayName, StringComparer.Ordinal);
        if (scope == ActorLibraryScope.Favourites)
            source = source.Where(item => item.IsFavorite);
        if (!string.Equals(category, allCategories, StringComparison.Ordinal))
            source = source.Where(item => string.Equals(item.Category, category, StringComparison.Ordinal));
        if (query.Length != 0)
        {
            source = source.Where(item => item.DisplayName.Contains(query, StringComparison.OrdinalIgnoreCase)
                || item.BlueprintReference.Contains(query, StringComparison.OrdinalIgnoreCase));
        }

        ActorQueueItemViewModel[] nextItems = source.ToArray();
        refreshing = true;
        HashSet<ActorQueueItemViewModel> retained = nextItems.ToHashSet();
        for (int index = Items.Count - 1; index >= 0; index--)
        {
            if (!retained.Contains(Items[index]))
            {
                Items[index].DeactivatePreviews();
                Items.RemoveAt(index);
            }
        }
        for (int index = 0; index < nextItems.Length; index++)
        {
            ActorQueueItemViewModel item = nextItems[index];
            if (index < Items.Count && ReferenceEquals(Items[index], item))
                continue;
            int previous = Items.IndexOf(item);
            if (previous >= 0)
                Items.Move(previous, index);
            else
                Items.Insert(index, item);
        }
        SelectedItem = selectedReference is null
            ? null
            : Items.FirstOrDefault(item => item.BlueprintReference == selectedReference);
        refreshing = false;
    }

    private void requestVisibleItemsRefresh()
    {
        if (!deferVisibleRefresh)
            refreshVisibleItems();
    }
}
