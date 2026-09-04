using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Threading;
using Avalonia.VisualTree;
using AvaloniaEdit;
using AvaloniaEdit.Document;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Ludork.Views.Utils;

internal sealed class LogTextViewController : IDisposable
{
    private const int MaximumBatchCount = 512;
    private const int MaximumLineCount = 5000;
    private static readonly TimeSpan flushInterval = TimeSpan.FromMilliseconds(50);
    private readonly TextEditor editor;
    private readonly object syncRoot = new();
    private readonly Queue<string> pendingText = new();
    private readonly DispatcherTimer flushTimer = new() { Interval = flushInterval };
    private ScrollViewer? scrollViewer;
    private bool flushScheduled;
    private bool followsLatest = true;
    private bool viewportUpdatePending;
    private int viewportGeneration;
    private bool disposed;

    public LogTextViewController(TextEditor editor)
    {
        this.editor = editor;
        editor.Document.UndoStack.SizeLimit = 0;
        editor.Options.AllowScrollBelowDocument = false;
        editor.TextArea.TextView.Margin = new Thickness(12, 8, 12, 16);
        flushTimer.Tick += onFlushTimer;
    }

    public void Append(string text)
    {
        if (text.Length == 0)
            return;
        bool scheduleFlush = false;
        lock (syncRoot)
        {
            if (disposed)
                return;
            pendingText.Enqueue(text);
            if (!flushScheduled)
            {
                flushScheduled = true;
                scheduleFlush = true;
            }
        }
        if (scheduleFlush)
        {
            Dispatcher.UIThread.Post(
                startFlushTimer,
                DispatcherPriority.Background);
        }
    }

    public void AppendLine(string line)
    {
        Append(line + Environment.NewLine);
    }

    public void Clear()
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(Clear);
            return;
        }
        flushTimer.Stop();
        lock (syncRoot)
        {
            pendingText.Clear();
            flushScheduled = false;
        }
        viewportGeneration++;
        viewportUpdatePending = true;
        editor.Clear();
        ScrollViewer? currentScrollViewer = findScrollViewer();
        if (currentScrollViewer is not null)
            currentScrollViewer.Offset = default;
        viewportUpdatePending = false;
        followsLatest = true;
    }

    public void Dispose()
    {
        lock (syncRoot)
        {
            if (disposed)
                return;
            disposed = true;
            pendingText.Clear();
            flushScheduled = false;
        }
        if (Dispatcher.UIThread.CheckAccess())
            disposeOnUiThread();
        else
            Dispatcher.UIThread.Post(disposeOnUiThread);
    }

    private void startFlushTimer()
    {
        lock (syncRoot)
        {
            if (disposed || !flushScheduled)
                return;
        }
        flushTimer.Start();
    }

    private void onFlushTimer(object? sender, EventArgs args)
    {
        flushTimer.Stop();
        StringBuilder batch = new();
        lock (syncRoot)
        {
            if (disposed)
                return;
            int count = Math.Min(pendingText.Count, MaximumBatchCount);
            for (int index = 0; index < count; index++)
                batch.Append(pendingText.Dequeue());
        }

        if (batch.Length > 0)
            appendToDocument(batch.ToString());

        bool restartTimer;
        lock (syncRoot)
        {
            restartTimer = !disposed && pendingText.Count > 0;
            if (!restartTimer)
                flushScheduled = false;
        }
        if (restartTimer)
            flushTimer.Start();
    }

    private void appendToDocument(string text)
    {
        ScrollViewer? currentScrollViewer = findScrollViewer();
        Vector previousOffset = currentScrollViewer?.Offset ?? default;
        bool followBottom = followsLatest;
        TextDocument document = editor.Document;

        viewportUpdatePending = true;
        using (document.RunUpdate())
        {
            document.Insert(document.TextLength, text);
            bool endsWithLineBreak = document.TextLength > 0
                && document.GetCharAt(document.TextLength - 1) is '\r' or '\n';
            int retainedDocumentLineCount = MaximumLineCount + (endsWithLineBreak ? 1 : 0);
            int removeLineCount = Math.Max(
                0,
                document.LineCount - retainedDocumentLineCount);
            if (removeLineCount > 0)
            {
                DocumentLine firstRemainingLine = document.GetLineByNumber(removeLineCount + 1);
                document.Remove(0, firstRemainingLine.Offset);
            }
        }

        int generation = ++viewportGeneration;
        Dispatcher.UIThread.Post(
            () => restoreViewport(generation, previousOffset, followBottom),
            DispatcherPriority.Background);
    }

    private ScrollViewer? findScrollViewer()
    {
        if (scrollViewer is not null)
            return scrollViewer;
        scrollViewer = editor.GetVisualDescendants()
            .OfType<ScrollViewer>()
            .FirstOrDefault(viewer => viewer.Name == "PART_ScrollViewer")
            ?? editor.GetVisualDescendants()
                .OfType<ScrollViewer>()
                .FirstOrDefault();
        if (scrollViewer is not null)
            scrollViewer.ScrollChanged += onScrollChanged;
        return scrollViewer;
    }

    private void onScrollChanged(object? sender, ScrollChangedEventArgs args)
    {
        if (viewportUpdatePending || sender is not ScrollViewer currentScrollViewer)
            return;
        double maximumY = Math.Max(
            0,
            currentScrollViewer.Extent.Height - currentScrollViewer.Viewport.Height);
        if (currentScrollViewer.Offset.Y >= maximumY - 2)
            followsLatest = true;
        else if (args.OffsetDelta.Y < 0)
            followsLatest = false;
    }

    private void restoreViewport(int generation, Vector previousOffset, bool followBottom)
    {
        if (disposed || generation != viewportGeneration)
            return;
        ScrollViewer? currentScrollViewer = findScrollViewer();
        if (currentScrollViewer is null)
        {
            viewportUpdatePending = false;
            return;
        }
        double maximumX = Math.Max(
            0,
            currentScrollViewer.Extent.Width - currentScrollViewer.Viewport.Width);
        double maximumY = Math.Max(
            0,
            currentScrollViewer.Extent.Height - currentScrollViewer.Viewport.Height);
        double targetX = Math.Clamp(previousOffset.X, 0, maximumX);
        double targetY = followBottom
            ? maximumY
            : Math.Clamp(previousOffset.Y, 0, maximumY);
        currentScrollViewer.Offset = new Vector(targetX, targetY);
        viewportUpdatePending = false;
    }

    private void disposeOnUiThread()
    {
        flushTimer.Stop();
        flushTimer.Tick -= onFlushTimer;
        if (scrollViewer is not null)
            scrollViewer.ScrollChanged -= onScrollChanged;
        scrollViewer = null;
    }
}
