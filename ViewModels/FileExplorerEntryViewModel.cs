using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.IO;

namespace Ludork.ViewModels;

public sealed class FileExplorerEntryViewModel : ViewModelBase, IDisposable
{
    private IImage? fallback;
    private ActorPreviewLease? previewLease;
    private IImage? icon;
    private long previewFrameRevision;
    private bool disposed;

    public FileExplorerEntryViewModel(string fullPath, bool isDirectory, IImage? icon)
        : this(fullPath, isDirectory, icon, null)
    {
    }

    public FileExplorerEntryViewModel(
        string fullPath,
        bool isDirectory,
        IImage? fallback,
        ActorPreviewLease? previewLease)
    {
        FullPath = fullPath;
        IsDirectory = isDirectory;
        this.fallback = fallback;
        icon = fallback;
        this.previewLease = previewLease;
        Name = Path.GetFileName(fullPath);
        if (previewLease is not null)
        {
            previewLease.FrameChanged += onPreviewFrameChanged;
            updateIcon();
        }
    }

    public string FullPath { get; }
    public bool IsDirectory { get; }
    public IImage? Icon
    {
        get => icon;
        private set => SetProperty(ref icon, value);
    }
    public long PreviewFrameRevision
    {
        get => previewFrameRevision;
        private set => SetProperty(ref previewFrameRevision, value);
    }
    public string Name { get; }

    public bool IsPreviewActive
    {
        get => previewLease?.IsActive == true;
        set
        {
            if (previewLease is not null)
                previewLease.IsActive = value;
        }
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
            updatePreviewFrame();
            return;
        }
        Dispatcher.UIThread.Post(updatePreviewFrame);
    }

    private void updatePreviewFrame()
    {
        if (disposed)
            return;
        updateIcon();
        PreviewFrameRevision += 1;
    }

    private void updateIcon()
    {
        if (disposed)
            return;
        IImage? next = previewLease?.Frame ?? fallback ?? icon;
        Icon = next;
    }
}
