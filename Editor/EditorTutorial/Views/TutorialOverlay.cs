using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.EditorTutorial.Models;
using Ludork.EditorTutorial.Playback;
using System;
using System.Collections.Generic;

namespace Ludork.EditorTutorial.Views;

internal sealed class TutorialOverlay : Control, IDisposable
{
    private readonly TutorialScene scene;
    private readonly TutorialPlaybackContext? playbackContext;
    private readonly List<TutorialVisualElement> elements = [];
    private bool presented;
    private bool dismissed;
    private Rect anchor;
    private double time;

    public TutorialOverlay(TutorialScene scene, TutorialPlaybackContext? playbackContext = null)
    {
        this.scene = scene;
        this.playbackContext = playbackContext;
        Focusable = true;
        ClipToBounds = true;
    }

    public event EventHandler? Presented;

    public void Prepare()
    {
        foreach (TutorialElement definition in scene.Elements)
        {
            TutorialVisualElement element = new(definition, scene, playbackContext);
            elements.Add(element);
            addChildren(element, definition);
        }
    }

    private void addChildren(TutorialVisualElement parent, TutorialElement definition)
    {
        foreach (TutorialElement childDefinition in definition.Children)
        {
            TutorialVisualElement child = new(childDefinition, scene, playbackContext);
            parent.AddChild(child);
            addChildren(child, childDefinition);
        }
    }

    public bool SetFrame(Rect bounds, double elapsed)
    {
        if (playbackContext is not null && !playbackContext.UpdateLayouts(bounds, scene.Elements))
            return false;
        double nextTime = TutorialTimeline.NormalizeTime(scene, elapsed);
        anchor = bounds;
        time = nextTime;
        InvalidateVisual();
        return true;
    }

    public void Dismiss()
    {
        dismissed = true;
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        if (dismissed)
            return;
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(165, 45, 45, 45)), new Rect(Bounds.Size));
        using (context.PushClip(anchor))
        using (context.PushTransform(Matrix.CreateTranslation(anchor.X, anchor.Y)))
            foreach (TutorialVisualElement element in elements)
                element.Draw(context, anchor.Size, time);
        if (!presented && anchor.Width > 0 && anchor.Height > 0)
        {
            presented = true;
            EventHandler? handler = Presented;
            Dispatcher.UIThread.Post(() => handler?.Invoke(this, EventArgs.Empty));
        }
    }

    public void Dispose()
    {
        foreach (TutorialVisualElement element in elements)
            element.Dispose();
        elements.Clear();
        playbackContext?.Dispose();
        Presented = null;
    }
}
