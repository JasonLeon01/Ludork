using Avalonia;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.EditorTutorial.Models;
using Ludork.EditorTutorial.Playback;
using Ludork.Models;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow
{
    private TutorialPlayer? tutorials;
    private IPointer? tutorialOutlinerPointer;
    private MapActorReference? tutorialOutlinerActor;
    private Key? tutorialOutlinerKey;
    private MapActorReference? tutorialOutlinerBeforeKey;
    private MapActorReference? tutorialOutlinerKeyActor;

    private void initializeTutorials()
    {
        if (projectSession is null || editorSettings is null)
            return;
        tutorials = new TutorialPlayer(this, WindowRoot, WindowContent, projectSession.TutorialTriggers,
            editorSettings, appendConsoleLine);
        tutorials.RegisterAnchor("map.canvas", getTutorialCanvasBounds);
        tutorials.RegisterAnchor("map.workspace", getTutorialWorkspaceBounds);
        tutorials.RegisterContext("map.tilePaintCommitted", createTileDeselectTutorialContext);
        tutorials.RegisterContext("map.actorSelectionCommitted", createActorTutorialContext);
        RightList.SelectionCommitted += onTutorialTileSelection;
        EditorPanel.TilePaintCommitted += onTutorialTilePaint;
        EditorPanel.ActorSelectionCommitted += onTutorialActorSelection;
        ActorOutliner.AddHandler(PointerPressedEvent, onTutorialOutlinerPressed, RoutingStrategies.Tunnel, true);
        ActorOutliner.AddHandler(PointerReleasedEvent, onTutorialOutlinerReleased, RoutingStrategies.Bubble, true);
        ActorOutliner.PointerCaptureLost += onTutorialOutlinerCaptureLost;
        ActorOutliner.AddHandler(KeyDownEvent, onTutorialOutlinerKeyDown, RoutingStrategies.Tunnel);
        ActorOutliner.AddHandler(KeyDownEvent, onTutorialOutlinerKeySelected, RoutingStrategies.Bubble, true);
        ActorOutliner.AddHandler(KeyUpEvent, onTutorialOutlinerKeyUp, RoutingStrategies.Bubble, true);
        ActorOutliner.LostFocus += onTutorialOutlinerLostFocus;
    }

    private Rect? getTutorialCanvasBounds()
    {
        if (!EditorScroll.IsEffectivelyVisible || !EditorPanel.IsEffectivelyVisible
            || WorldEditorPanel.IsVisible || GameViewport.IsVisible
            || viewModel?.MapWorkspace.SelectedMap is not { IsWorld: false }
            || EditorPanel.GetVisibleMapBounds() is not { } bounds)
            return null;
        Point? origin = EditorPanel.TranslatePoint(bounds.Position, WindowRoot);
        if (origin is not { } point)
            return null;
        return new Rect(point, bounds.Size).Intersect(new Rect(WindowRoot.Bounds.Size));
    }

    private void onTutorialTileSelection(object? sender, EventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (tutorials is null || viewModel?.CanEdit != true
                || viewModel.MapWorkspace.IsSelectedLayerEditable != true
                || EditorPanel.EditMode != MapEditMode.Tile
                || tileSelect is null || tileSelect.SelectedTiles is null && tileSelect.SelectedAutoTile is null)
                return;
            projectSession?.TutorialTriggers.Publish("map.tileSelectionCommitted");
        }, DispatcherPriority.Background);
    }

    private Rect? getTutorialWorkspaceBounds()
    {
        if (getTutorialCanvasBounds() is null || !RightList.IsEffectivelyVisible
            || WindowContent.TranslatePoint(default, WindowRoot) is not { } origin)
            return null;
        return new Rect(origin, WindowContent.Bounds.Size).Intersect(new Rect(WindowRoot.Bounds.Size));
    }

    private bool canShowTileDeselectTutorial() => viewModel?.CanEdit == true
        && viewModel.MapWorkspace.IsSelectedLayerEditable && !EditorPanel.IsRuntimeEditing
        && EditorPanel.EditMode == MapEditMode.Tile
        && tileSelect is { SelectedTiles: { Width: 1, Height: 1 }, SelectedAutoTile: null }
        && getTutorialWorkspaceBounds() is not null;

    private bool matchesTileTutorialSelection(string? mapKey, string? layerName,
        TilesetTabViewModel? tileset, TileSelection? selection) => canShowTileDeselectTutorial()
        && EditorPanel.CurrentMapKey == mapKey
        && viewModel?.MapWorkspace.SelectedLayerTab?.Name == layerName
        && tileSelect?.SelectedTileset == tileset && tileSelect?.SelectedTiles == selection;

    private void onTutorialTilePaint(object? sender, EventArgs args)
    {
        if (!canShowTileDeselectTutorial())
            return;
        string? mapKey = EditorPanel.CurrentMapKey;
        string? layerName = viewModel?.MapWorkspace.SelectedLayerTab?.Name;
        TilesetTabViewModel? tileset = tileSelect?.SelectedTileset;
        TileSelection? selection = tileSelect?.SelectedTiles;
        Dispatcher.UIThread.Post(() =>
        {
            if (tutorials is not null && matchesTileTutorialSelection(mapKey, layerName, tileset, selection))
                projectSession?.TutorialTriggers.Publish("map.tilePaintCommitted");
        }, DispatcherPriority.Background);
    }

    private Task<TutorialPlaybackContext?> createTileDeselectTutorialContext(CancellationToken cancellationToken)
    {
        if (cancellationToken.IsCancellationRequested || !canShowTileDeselectTutorial())
            return Task.FromResult<TutorialPlaybackContext?>(null);
        string? mapKey = EditorPanel.CurrentMapKey;
        string? layerName = viewModel?.MapWorkspace.SelectedLayerTab?.Name;
        TilesetTabViewModel? tileset = tileSelect?.SelectedTileset;
        TileSelection? selection = tileSelect?.SelectedTiles;
        TutorialPlaybackContext context = new(new Dictionary<string, Bitmap>(), anchor =>
            matchesTileTutorialSelection(mapKey, layerName, tileset, selection)
                ? getTileDeselectTutorialLayouts(anchor) : null);
        return Task.FromResult<TutorialPlaybackContext?>(context);
    }

    private IReadOnlyDictionary<string, TutorialLayout>? getTileDeselectTutorialLayouts(Rect anchor)
    {
        if (RightList.GetSelectedTileBounds(WindowRoot) is not { } bounds
            || anchor.Width < 392 || anchor.Height < 212)
            return null;
        Rect tile = bounds.Intersect(anchor);
        if (tile.Width <= 0 || tile.Height <= 0)
            return null;
        const double hintWidth = 360;
        const double hintHeight = 180;
        double hintX = Math.Clamp(tile.Left - anchor.Left - hintWidth - 24, 16, anchor.Width - hintWidth - 16);
        double hintY = Math.Clamp(tile.Center.Y - anchor.Top - hintHeight / 2, 16, anchor.Height - hintHeight - 16);
        return new Dictionary<string, TutorialLayout>
        {
            ["selectedTile"] = new(tile.Size, Matrix.CreateTranslation(tile.X - anchor.X, tile.Y - anchor.Y)),
            ["instructions"] = new(new Size(hintWidth, hintHeight), Matrix.CreateTranslation(hintX, hintY))
        };
    }

    private bool canShowActorTutorial() => viewModel?.CanEdit == true
        && viewModel.MapWorkspace.IsSelectedLayerEditable && !EditorPanel.IsRuntimeEditing
        && EditorPanel.EditMode == MapEditMode.Actor && EditorPanel.SelectedActors.Count == 1
        && getTutorialCanvasBounds() is not null;

    private async Task<TutorialPlaybackContext?> createActorTutorialContext(CancellationToken cancellationToken)
    {
        if (!canShowActorTutorial())
            return null;
        MapActorReference actor = EditorPanel.SelectedActors[0];
        string? mapKey = EditorPanel.CurrentMapKey;
        if (await EditorPanel.CaptureSelectedActorImageAsync(cancellationToken) is not { } snapshot)
            return null;
        (RenderTargetBitmap image, Vector frameOffset) = snapshot;
        if (cancellationToken.IsCancellationRequested || !canShowActorTutorial()
            || EditorPanel.CurrentMapKey != mapKey || EditorPanel.SelectedActors[0] != actor)
        {
            image.Dispose();
            return null;
        }
        Size imageSize = new(image.PixelSize.Width, image.PixelSize.Height);
        return new TutorialPlaybackContext(new Dictionary<string, Bitmap> { ["selectedActor"] = image },
            anchor => getActorTutorialLayouts(actor, mapKey, imageSize, frameOffset, anchor));
    }

    private IReadOnlyDictionary<string, TutorialLayout>? getActorTutorialLayouts(
        MapActorReference actor, string? mapKey, Size imageSize, Vector frameOffset, Rect anchor)
    {
        if (!canShowActorTutorial() || EditorPanel.CurrentMapKey != mapKey || EditorPanel.SelectedActors[0] != actor
            || EditorPanel.GetSelectedActorImageLayout(frameOffset) is not { } layout
            || EditorPanel.TransformToVisual(WindowRoot) is not { } panelToRoot)
            return null;
        Matrix imageToRoot = layout.Transform * panelToRoot;
        Rect visible = new Rect(imageSize).TransformToAABB(imageToRoot).Intersect(anchor);
        if (visible.Width <= 0 || visible.Height <= 0 || !intersectsTutorialImage(imageSize, imageToRoot, anchor))
            return null;
        Point pivot = panelToRoot.Transform(layout.Pivot);
        Matrix relativeToPivot = Matrix.CreateTranslation(-pivot.X, -pivot.Y);
        Matrix imageToPivot = imageToRoot * relativeToPivot;
        Rect grid = layout.GridBounds.TransformToAABB(panelToRoot * relativeToPivot);
        Point contact = imageToPivot.Transform(new Point(imageSize.Width / 2, imageSize.Height / 2));
        double hintWidth = Math.Max(96, anchor.Width * 0.27);
        double hintX = visible.Center.X < anchor.Center.X ? anchor.Width - hintWidth - 16 : 16;
        double hintY = Math.Clamp(anchor.Height * 0.26, 52, Math.Max(52, anchor.Height - 292));
        return new Dictionary<string, TutorialLayout>
        {
            ["actorPlacement"] = new(imageSize, Matrix.CreateTranslation(pivot.X - anchor.X, pivot.Y - anchor.Y)),
            ["actorImage"] = new(imageSize, imageToPivot),
            ["actorGrid"] = new(grid.Size, Matrix.CreateTranslation(grid.X, grid.Y)),
            ["actorContact"] = new(new Size(48, 64), Matrix.CreateTranslation(contact.X, contact.Y)),
            ["inputHint"] = new(new Size(hintWidth, 186), Matrix.CreateTranslation(hintX, hintY))
        };
    }

    private static bool intersectsTutorialImage(Size size, Matrix transform, Rect anchor)
    {
        Rect image = new(size);
        Point[] corners = [transform.Transform(image.TopLeft), transform.Transform(image.TopRight),
            transform.Transform(image.BottomRight), transform.Transform(image.BottomLeft)];
        Point[] clip = [anchor.TopLeft, anchor.TopRight, anchor.BottomRight, anchor.BottomLeft];
        foreach (Vector edge in new[] { corners[1] - corners[0], corners[3] - corners[0] })
        {
            Vector axis = new(-edge.Y, edge.X);
            double imageMin = double.PositiveInfinity;
            double imageMax = double.NegativeInfinity;
            double clipMin = double.PositiveInfinity;
            double clipMax = double.NegativeInfinity;
            for (int index = 0; index < 4; index++)
            {
                double imageProjection = corners[index].X * axis.X + corners[index].Y * axis.Y;
                double clipProjection = clip[index].X * axis.X + clip[index].Y * axis.Y;
                imageMin = Math.Min(imageMin, imageProjection);
                imageMax = Math.Max(imageMax, imageProjection);
                clipMin = Math.Min(clipMin, clipProjection);
                clipMax = Math.Max(clipMax, clipProjection);
            }
            if (imageMax <= clipMin || clipMax <= imageMin)
                return false;
        }
        return true;
    }

    private void onTutorialActorSelection(object? sender, EventArgs args)
    {
        if (!canShowActorTutorial())
            return;
        MapActorReference actor = EditorPanel.SelectedActors[0];
        string? mapKey = EditorPanel.CurrentMapKey;
        Dispatcher.UIThread.Post(() =>
        {
            if (tutorials is not null && canShowActorTutorial() && EditorPanel.CurrentMapKey == mapKey
                && EditorPanel.SelectedActors[0] == actor)
                projectSession?.TutorialTriggers.Publish("map.actorSelectionCommitted");
        }, DispatcherPriority.Background);
    }

    private void beginTutorialOutlinerSelection(PointerPressedEventArgs args, MapActorReference? actor)
    {
        PointerPointProperties properties = args.GetCurrentPoint(ActorOutliner).Properties;
        if (actor is null || !properties.IsLeftButtonPressed || hasSecondaryTutorialButton(properties))
            return;
        tutorialOutlinerActor = actor;
        tutorialOutlinerPointer = args.Pointer;
        args.Pointer.Capture(ActorOutliner);
    }

    private static bool hasSecondaryTutorialButton(PointerPointProperties properties) =>
        properties.IsRightButtonPressed || properties.IsMiddleButtonPressed
        || properties.IsXButton1Pressed || properties.IsXButton2Pressed;

    private void onTutorialOutlinerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (args.Pointer == tutorialOutlinerPointer
            && hasSecondaryTutorialButton(args.GetCurrentPoint(ActorOutliner).Properties))
        {
            tutorialOutlinerActor = null;
            tutorialOutlinerPointer = null;
            if (args.Pointer.Captured == ActorOutliner)
                args.Pointer.Capture(null);
        }
    }

    private void onTutorialOutlinerReleased(object? sender, PointerReleasedEventArgs args)
    {
        if (args.Pointer != tutorialOutlinerPointer || args.InitialPressMouseButton != MouseButton.Left)
            return;
        MapActorReference? actor = tutorialOutlinerActor;
        tutorialOutlinerActor = null;
        tutorialOutlinerPointer = null;
        if (args.Pointer.Captured == ActorOutliner)
            args.Pointer.Capture(null);
        if (actor is not null && actor == EditorPanel.PrimarySelectedActor
            && !hasSecondaryTutorialButton(args.GetCurrentPoint(ActorOutliner).Properties))
            onTutorialActorSelection(sender, EventArgs.Empty);
    }

    private void onTutorialOutlinerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        tutorialOutlinerActor = null;
        tutorialOutlinerPointer = null;
    }

    private static bool isTutorialSelectionKey(Key key) => key is Key.Up or Key.Down or Key.Left or Key.Right
        or Key.Home or Key.End or Key.PageUp or Key.PageDown or Key.Space or Key.Enter;

    private void onTutorialOutlinerKeyDown(object? sender, KeyEventArgs args)
    {
        if (tutorialOutlinerKey is not null || !isTutorialSelectionKey(args.Key))
            return;
        tutorialOutlinerKey = args.Key;
        tutorialOutlinerBeforeKey = EditorPanel.PrimarySelectedActor;
        tutorialOutlinerKeyActor = null;
    }

    private void onTutorialOutlinerKeySelected(object? sender, KeyEventArgs args)
    {
        if (tutorialOutlinerKey == args.Key && tutorialOutlinerBeforeKey != EditorPanel.PrimarySelectedActor)
            tutorialOutlinerKeyActor = EditorPanel.PrimarySelectedActor;
    }

    private void onTutorialOutlinerKeyUp(object? sender, KeyEventArgs args)
    {
        if (tutorialOutlinerKey != args.Key)
            return;
        bool changed = tutorialOutlinerKeyActor is not null
            && tutorialOutlinerKeyActor == EditorPanel.PrimarySelectedActor;
        tutorialOutlinerKey = null;
        tutorialOutlinerBeforeKey = null;
        tutorialOutlinerKeyActor = null;
        if (changed)
            onTutorialActorSelection(sender, EventArgs.Empty);
    }

    private void onTutorialOutlinerLostFocus(object? sender, RoutedEventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (!ActorOutliner.IsKeyboardFocusWithin)
            {
                tutorialOutlinerKey = null;
                tutorialOutlinerBeforeKey = null;
                tutorialOutlinerKeyActor = null;
            }
        });
    }

    private void disposeTutorials()
    {
        RightList.SelectionCommitted -= onTutorialTileSelection;
        EditorPanel.TilePaintCommitted -= onTutorialTilePaint;
        EditorPanel.ActorSelectionCommitted -= onTutorialActorSelection;
        ActorOutliner.RemoveHandler(PointerPressedEvent, onTutorialOutlinerPressed);
        ActorOutliner.RemoveHandler(PointerReleasedEvent, onTutorialOutlinerReleased);
        ActorOutliner.PointerCaptureLost -= onTutorialOutlinerCaptureLost;
        ActorOutliner.RemoveHandler(KeyDownEvent, onTutorialOutlinerKeyDown);
        ActorOutliner.RemoveHandler(KeyDownEvent, onTutorialOutlinerKeySelected);
        ActorOutliner.RemoveHandler(KeyUpEvent, onTutorialOutlinerKeyUp);
        ActorOutliner.LostFocus -= onTutorialOutlinerLostFocus;
        tutorialOutlinerPointer?.Capture(null);
        tutorialOutlinerPointer = null;
        tutorialOutlinerActor = null;
        tutorialOutlinerKey = null;
        tutorialOutlinerBeforeKey = null;
        tutorialOutlinerKeyActor = null;
        tutorials?.Dispose();
        tutorials = null;
    }
}
