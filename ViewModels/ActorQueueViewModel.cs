using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using System.Text.Json.Nodes;
using Ludork.Services;
using System;
using System.Collections.ObjectModel;
using System.Linq;

namespace Ludork.ViewModels;

public sealed partial class ActorQueueViewModel : ViewModelBase, IDisposable
{
    private readonly GameDataService gameData;
    private readonly BlueprintPreviewService previewService;
    private readonly FileIconService iconService;
    private bool previewsActive;
    private bool disposed;
    [ObservableProperty] private ActorQueueItemViewModel? selectedItem;

    public ActorQueueViewModel(GameDataService gameData, BlueprintPreviewService previewService, FileIconService iconService)
    {
        this.gameData = gameData;
        this.previewService = previewService;
        this.iconService = iconService;
        gameData.DataReloaded += onDataReloaded;
    }

    public ObservableCollection<ActorQueueItemViewModel> Items { get; } = [];
    public event EventHandler<string?>? SelectionChanged;
    public event EventHandler<string>? BlueprintOpenRequested;
    public event EventHandler<string>? BlueprintLocateRequested;

    partial void OnSelectedItemChanged(ActorQueueItemViewModel? value) => SelectionChanged?.Invoke(this, value?.BlueprintReference);

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
        ActorQueueItemViewModel? existing = Items.FirstOrDefault(item => item.BlueprintReference == blueprintReference);
        if (existing is not null)
        {
            Items.Remove(existing);
            existing.Dispose();
        }
        string key = blueprintReference["Data.Blueprints.".Length..].Replace('.', '/');
        gameData.BlueprintsData.TryGetValue(key, out JsonObject? data);
        Bitmap? fallback = previewService.tryLoadPreview(data ?? [], 48, key);
        ActorVisualDescriptor? descriptor = previewService.tryResolveActorVisual(blueprintReference);
        ActorQueueItemViewModel item = new ActorQueueItemViewModel(
            blueprintReference,
            fallback ?? iconService.getShellIcon(key + ".json", false, 48),
            descriptor is { RequiresPreviewService: true }
                ? previewService.ActorPreviews.Acquire(descriptor, 48, previewsActive)
                : null);
        item.IsPreviewActive = previewsActive;
        Items.Insert(0, item);
        SelectedItem = item;
    }

    public void Remove(ActorQueueItemViewModel item, ActorQueueItemViewModel? previousItem = null)
    {
        if (previousItem is not null && previousItem != item && Items.Contains(previousItem))
            Select(previousItem);
        if (SelectedItem == item)
            Select(null);
        Items.Remove(item);
        item.Dispose();
    }

    public void PurgeStale()
    {
        foreach (ActorQueueItemViewModel item in Items.Where(item => !gameData.BlueprintsData.ContainsKey(item.Key)).ToArray())
        {
            Items.Remove(item);
            item.Dispose();
        }
        if (SelectedItem is not null && !Items.Contains(SelectedItem))
            SelectedItem = null;
    }

    public void RequestOpen(ActorQueueItemViewModel? item)
    {
        if (item is not null)
            BlueprintOpenRequested?.Invoke(this, item.BlueprintReference);
    }

    public void RequestLocate(ActorQueueItemViewModel? item)
    {
        if (item is not null)
            BlueprintLocateRequested?.Invoke(this, item.BlueprintReference);
    }

    public void SetPreviewActive(bool active)
    {
        previewsActive = active;
        foreach (ActorQueueItemViewModel item in Items)
            item.IsPreviewActive = active;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.DataReloaded -= onDataReloaded;
        foreach (ActorQueueItemViewModel item in Items)
            item.Dispose();
        Items.Clear();
        SelectedItem = null;
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        refreshPreviews();
    }

    private void refreshPreviews()
    {
        if (disposed)
            return;
        PurgeStale();
        foreach (ActorQueueItemViewModel item in Items)
        {
            gameData.BlueprintsData.TryGetValue(item.Key, out JsonObject? blueprint);
            ActorVisualDescriptor? descriptor = previewService.tryResolveActorVisual(item.BlueprintReference);
            Bitmap? fallback = previewService.tryLoadPreview(blueprint ?? [], 48, item.Key);
            item.UpdatePreview(
                descriptor is { RequiresPreviewService: true }
                    ? previewService.ActorPreviews.Acquire(descriptor, 48, previewsActive)
                    : null,
                fallback ?? iconService.getShellIcon(item.Key + ".json", false, 48));
        }
    }
}

public sealed class ActorQueueItemViewModel : ViewModelBase, IDisposable
{
    private IImage? fallback;
    private ActorPreviewLease? previewLease;
    private IImage? icon;
    private bool previewActive;
    private bool disposed;

    public ActorQueueItemViewModel(
        string blueprintReference,
        IImage? fallback,
        ActorPreviewLease? previewLease)
    {
        BlueprintReference = blueprintReference;
        this.fallback = fallback;
        icon = fallback;
        this.previewLease = previewLease;
        Key = blueprintReference["Data.Blueprints.".Length..].Replace('.', '/');
        DisplayName = Key.Split('/').LastOrDefault() ?? Key;
        if (previewLease is not null)
        {
            previewLease.FrameChanged += onPreviewFrameChanged;
            updateIcon();
        }
    }

    public string BlueprintReference { get; }
    public string Key { get; }
    public string DisplayName { get; }
    public IImage? Icon
    {
        get => icon;
        private set => SetProperty(ref icon, value);
    }

    public bool IsPreviewActive
    {
        get => previewActive;
        set
        {
            previewActive = value;
            if (previewLease is not null)
                previewLease.IsActive = value;
        }
    }

    public void UpdatePreview(
        ActorPreviewLease? nextLease,
        IImage? nextFallback)
    {
        IImage? previousFallback = fallback;
        ActorPreviewLease? previousLease = previewLease;
        fallback = nextFallback;
        if (previousLease is not null)
            previousLease.FrameChanged -= onPreviewFrameChanged;
        previewLease = nextLease;
        if (previewLease is not null)
        {
            previewLease.FrameChanged += onPreviewFrameChanged;
            previewLease.IsActive = previewActive;
        }
        updateIcon();
        previousLease?.Dispose();
        if (!ReferenceEquals(previousFallback, nextFallback))
            (previousFallback as IDisposable)?.Dispose();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        if (previewLease is not null)
        {
            previewLease.FrameChanged -= onPreviewFrameChanged;
            previewLease.Dispose();
            previewLease = null;
        }
        (fallback as IDisposable)?.Dispose();
        fallback = null;
        icon = null;
    }

    private void onPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
        {
            updateIcon();
            return;
        }
        Dispatcher.UIThread.Post(() => updateIcon());
    }

    private void updateIcon()
    {
        if (disposed)
            return;
        IImage? next = previewLease?.Frame ?? fallback;
        if (ReferenceEquals(Icon, next))
        {
            OnPropertyChanged(nameof(Icon));
            return;
        }
        Icon = next;
    }
}
