using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class ParticleEditor : UserControl
{
    private readonly GameDataService gameData;
    private readonly UiPreviewRuntimeService runtime;
    private readonly EditorDocument document;
    private readonly ListBox trackList = new();
    private readonly StackPanel properties = new() { Spacing = 8, Margin = new Thickness(8) };
    private readonly ContentControl curveHost = new();
    private readonly CheckBox solo = new() { Content = LocaleService.Get("PARTICLE_SOLO") };
    private JsonObject data;
    private int selectedTrack = -1;
    private bool refreshing;
    private bool committing;

    public ParticleEditor(GameDataService gameData, UiPreviewRuntimeService runtime, string key)
    {
        this.gameData = gameData;
        this.runtime = runtime;
        document = gameData.GetDocument("Particles", key) ?? throw new ArgumentException("Particle document was not found", nameof(key));
        data = document.Data!;
        trackList.SelectionChanged += (_, _) =>
        {
            if (refreshing)
                return;
            gameData.BreakHistoryGesture();
            selectedTrack = trackList.SelectedIndex;
            buildProperties();
            if (solo.IsChecked == true)
                requestLoad();
        };
        solo.IsCheckedChanged += (_, _) => requestLoad();
        buildLayout();
        refreshTracks(0);
    }

    public string Key => document.Key;
    public void PausePreview() => setPlaying(false);
    private JsonArray tracks => data["tracks"] as JsonArray ?? new JsonArray();
    private JsonObject? track => selectedTrack >= 0 && selectedTrack < tracks.Count ? tracks[selectedTrack] as JsonObject : null;

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        document.Changed += onDocumentChanged;
        initializePlayback();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        document.Changed -= onDocumentChanged;
        disposePlayback();
        base.OnDetachedFromVisualTree(args);
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (committing || document.Data is not JsonObject next)
            return;
        data = next;
        refreshTracks(selectedTrack);
        requestLoad();
    }

    private void buildLayout()
    {
        Grid root = new() { ColumnDefinitions = new ColumnDefinitions("170,5,*,5,350") };
        DockPanel trackPanel = new() { Margin = new Thickness(6) };
        StackPanel trackButtons = new() { Spacing = 5 };
        trackButtons.Children.Add(button("PARTICLE_ADD_TRACK", () =>
        {
            if (data["tracks"] is not JsonArray)
                data["tracks"] = new JsonArray();
            JsonObject created = ParticleAssetSchema.CreateTrack(uniqueTrackName(LocaleService.Get("PARTICLE_TRACK")));
            string glow = "/Game/Assets/Particles/Glow.png";
            if (GameAssetPath.TryResolveExistingFile(gameData.ProjectPath, glow, out _))
                created["texture"] = glow;
            tracks.Add(created);
            commit();
            refreshTracks(tracks.Count - 1);
        }));
        trackButtons.Children.Add(button("PARTICLE_DUPLICATE_TRACK", () =>
        {
            if (track is not JsonObject current)
                return;
            JsonObject copy = (JsonObject)current.DeepClone();
            copy["name"] = uniqueTrackName(text(current, "name") + " (copy)");
            tracks.Insert(selectedTrack + 1, copy);
            commit();
            refreshTracks(selectedTrack + 1);
        }));
        trackButtons.Children.Add(button("PARTICLE_REMOVE_TRACK", () =>
        {
            if (track is null)
                return;
            tracks.RemoveAt(selectedTrack);
            commit();
            refreshTracks(selectedTrack);
        }));
        StackPanel reorder = new() { Orientation = Orientation.Horizontal, Spacing = 5 };
        reorder.Children.Add(button("PARTICLE_MOVE_UP", () => moveTrack(-1)));
        reorder.Children.Add(button("PARTICLE_MOVE_DOWN", () => moveTrack(1)));
        trackButtons.Children.Add(reorder);
        trackButtons.Children.Add(solo);
        DockPanel.SetDock(trackButtons, Dock.Bottom);
        trackPanel.Children.Add(trackButtons);
        trackPanel.Children.Add(trackList);
        root.Children.Add(trackPanel);
        Grid preview = new() { RowDefinitions = new RowDefinitions("*,Auto,260") };
        preview.Children.Add(createPreview());
        Control playback = createPlaybackToolbar();
        Grid.SetRow(playback, 1);
        preview.Children.Add(playback);
        Grid.SetRow(curveHost, 2);
        preview.Children.Add(curveHost);
        Grid.SetColumn(preview, 2);
        root.Children.Add(preview);
        ScrollViewer inspector = new() { Content = properties, HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled };
        Grid.SetColumn(inspector, 4);
        root.Children.Add(inspector);
        foreach (int column in new[] { 1, 3 })
        {
            GridSplitter splitter = new() { Width = 5, HorizontalAlignment = HorizontalAlignment.Stretch, Background = Brushes.DimGray };
            Grid.SetColumn(splitter, column);
            root.Children.Add(splitter);
        }
        Content = root;
    }

    private void refreshTracks(int selection)
    {
        refreshing = true;
        trackList.ItemsSource = tracks.OfType<JsonObject>().Select(item =>
            (item["enabled"]?.GetValue<bool>() == false ? "○ " : "● ") + text(item, "name")).ToArray();
        selectedTrack = tracks.Count == 0 ? -1 : Math.Clamp(selection, 0, tracks.Count - 1);
        trackList.SelectedIndex = selectedTrack;
        refreshing = false;
        buildProperties();
    }

    private void moveTrack(int direction)
    {
        int destination = selectedTrack + direction;
        if (track is null || destination < 0 || destination >= tracks.Count)
            return;
        JsonNode? current = tracks[selectedTrack];
        tracks.RemoveAt(selectedTrack);
        tracks.Insert(destination, current);
        commit();
        refreshTracks(destination);
    }

    private string uniqueTrackName(string name)
    {
        string result = name;
        int index = 1;
        while (tracks.OfType<JsonObject>().Any(track => text(track, "name") == result))
            result = name + " " + index++;
        return result;
    }

    private void commit()
    {
        committing = true;
        gameData.UpdateParticle(Key, data);
        committing = false;
        requestLoad();
    }

    private static Button button(string locale, Action action)
    {
        Button result = new() { Content = LocaleService.Get(locale), HorizontalAlignment = HorizontalAlignment.Stretch };
        result.Click += (_, _) => action();
        return result;
    }

    private static string text(JsonObject value, string property, string fallback = "") => value[property]?.GetValue<string>() ?? fallback;
    private static double number(JsonNode? value, double fallback = 0) => ParticleAssetSchema.Number(value, fallback);
}
