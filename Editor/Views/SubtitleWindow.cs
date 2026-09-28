using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed partial class SubtitleWindow : Window, IProjectSaveParticipant
{
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly UiPreviewRuntimeService runtime;
    private readonly EditorDocument document;
    private readonly EditorDocumentBinding binding;
    private readonly Toast toast;
    private readonly StackPanel properties = new() { Spacing = 8, Margin = new Thickness(10) };
    private readonly TextBlock errors = new() { Foreground = Brushes.OrangeRed, TextWrapping = TextWrapping.Wrap };
    private readonly SubtitleTimeline timeline;
    private readonly Dictionary<Control, string> inputErrors = [];
    private JsonObject data;
    private int selected = -1;
    private string selectedLanguage = "en_GB";
    private bool committing;
    private bool refreshing;
    private bool confirmedClose;
    private bool confirmingClose;
    private long gesture;

    public SubtitleWindow(ProjectDataStore gameData, ProjectSaveService projectSave, string key, UiPreviewRuntimeService runtime)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.runtime = runtime;
        document = gameData.GetDocument("Subtitles", key) ?? throw new ArgumentException("Subtitle document was not found", nameof(key));
        data = document.Data!;
        Width = 1280;
        Height = 820;
        MinWidth = 900;
        MinHeight = 640;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(SubtitleWindow));
        timeline = new SubtitleTimeline(() => sections);
        timeline.SelectionChanged += index => select(index);
        timeline.Scrub += scrub;
        timeline.GestureStarted += () => gesture = gameData.BeginHistoryGesture();
        timeline.GestureEnded += () => { gameData.EndHistoryGesture(gesture); buildProperties(); };
        timeline.SegmentChanged += (index, start, end) =>
        {
            if (index >= 0 && index < sections.Count && sections[index] is JsonObject item)
            {
                item["startTime"] = start;
                item["endTime"] = end;
                commit();
            }
        };
        buildLayout();
        toast = new Toast(this);
        binding = new EditorDocumentBinding(this, gameData, () => document,
            () => L("SUBTITLE_EDITOR") + " - " + Key, closeWhenDeleted: true);
        binding.HasPendingInputs = () => inputErrors.Count != 0;
        projectSave.RegisterParticipant(this);
        document.Changed += documentChanged;
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        Opened += (_, _) => initializePreview();
        Closed += (_, _) =>
        {
            document.Changed -= documentChanged;
            projectSave.UnregisterParticipant(this);
            disposePreview();
        };
        Closing += async (_, args) =>
        {
            if (confirmedClose || args.CloseReason == WindowCloseReason.OwnerWindowClosing || (!document.IsModified && inputErrors.Count == 0))
                return;
            args.Cancel = true;
            if (confirmingClose)
                return;
            confirmingClose = true;
            bool close = await ConfirmationDialog.ShowAsync(this, L("SUBTITLE_EDITOR"),
                L(inputErrors.Count != 0 ? "SUBTITLE_CLOSE_INVALID" : "SUBTITLE_CLOSE_MODIFIED"));
            confirmingClose = false;
            if (close)
            {
                confirmedClose = true;
                Close();
            }
        };
        select(sections.Count == 0 ? -1 : 0);
        updateValidation();
    }

    public string Key => document.Key;
    public IReadOnlyList<string> PendingInputErrors => inputErrors.Values.Distinct().ToArray();
    public IReadOnlyList<string> PendingInputPaths => inputErrors.Count == 0 ? [] : [document.Path];
    public void FlushPendingChanges() { }
    public void PausePreview() => requestPreview("pause");
    private JsonArray sections => data["sections"] as JsonArray ?? new JsonArray();
    private JsonObject? section => selected >= 0 && selected < sections.Count ? sections[selected] as JsonObject : null;
    private static string L(string key) => LocaleService.Get(key);
    private static double number(JsonNode? value) => value is JsonValue scalar && scalar.TryGetValue(out double result) && double.IsFinite(result) ? result : 0;
    private static decimal decimalTime(double value) => value >= (double)decimal.MaxValue ? decimal.MaxValue : (decimal)Math.Max(0, value);

    private void buildLayout()
    {
        Grid root = new() { RowDefinitions = new RowDefinitions("*,Auto,145"), ColumnDefinitions = new ColumnDefinitions("*,5,360") };
        root.Children.Add(createPreview());
        GridSplitter splitter = new() { Width = 5, HorizontalAlignment = HorizontalAlignment.Stretch };
        Grid.SetColumn(splitter, 1);
        root.Children.Add(splitter);
        ScrollViewer inspector = new() { Content = properties };
        Grid.SetColumn(inspector, 2);
        root.Children.Add(inspector);
        Control toolbar = createPlaybackToolbar();
        Grid.SetColumnSpan(toolbar, 3);
        Grid.SetRow(toolbar, 1);
        root.Children.Add(toolbar);
        ScrollViewer timelineScroll = new()
        {
            Content = timeline,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled,
        };
        Grid.SetColumnSpan(timelineScroll, 3);
        Grid.SetRow(timelineScroll, 2);
        root.Children.Add(timelineScroll);
        Content = root;
    }

    private static Button button(string key, Action action)
    {
        Button result = new() { Content = L(key), Margin = new Thickness(0, 0, 5, 4) };
        result.Click += (_, _) => action();
        return result;
    }

    private Button editButton(string key, Action action) => button(key, () =>
    {
        if (inputErrors.Count == 0)
            action();
    });

    private void select(int index)
    {
        if (inputErrors.Count != 0)
        {
            timeline.SelectedIndex = selected;
            timeline.InvalidateVisual();
            return;
        }
        selected = index >= 0 && index < sections.Count ? index : -1;
        timeline.SelectedIndex = selected;
        timeline.InvalidateVisual();
        buildProperties();
    }

    private void buildProperties()
    {
        if (inputErrors.Count != 0)
            return;
        refreshing = true;
        properties.Children.Clear();
        WrapPanel actions = new();
        actions.Children.Add(editButton("SUBTITLE_ADD_SECTION", addSection));
        Button remove = editButton("SUBTITLE_REMOVE_SECTION", () =>
        {
            if (selected < 0 || selected >= sections.Count)
                return;
            sections.RemoveAt(selected);
            selected = Math.Min(selected, sections.Count - 1);
            commit();
            select(selected);
        });
        remove.IsEnabled = selected >= 0 && selected < sections.Count;
        actions.Children.Add(remove);
        properties.Children.Add(actions);
        properties.Children.Add(errors);
        if (section is not JsonObject current)
        {
            refreshing = false;
            return;
        }
        properties.Children.Add(new TextBlock { Text = L("SUBTITLE_SECTION") + " " + (selected + 1), FontWeight = FontWeight.Bold });
        addTimeField(current, "startTime", "SUBTITLE_START");
        addTimeField(current, "endTime", "SUBTITLE_END");
        CheckBox multilingual = new() { Content = L("SUBTITLE_MULTILINGUAL"), IsChecked = current["content"] is JsonObject };
        multilingual.IsCheckedChanged += async (_, _) =>
        {
            if (refreshing)
                return;
            if (inputErrors.Count != 0)
            {
                refreshing = true;
                multilingual.IsChecked = current["content"] is JsonObject;
                refreshing = false;
                return;
            }
            if (multilingual.IsChecked == true && current["content"] is not JsonObject)
            {
                current["content"] = new JsonObject { ["en_GB"] = current["content"] is JsonArray plain ? plain.DeepClone() : new JsonArray() };
                selectedLanguage = "en_GB";
            }
            else if (multilingual.IsChecked != true && current["content"] is JsonObject languages)
            {
                if (languages.Count > 1 && !await ConfirmationDialog.ShowAsync(this, L("SUBTITLE_MULTILINGUAL"),
                    L("SUBTITLE_KEEP_LANGUAGE") + " " + selectedLanguage))
                {
                    refreshing = true;
                    multilingual.IsChecked = true;
                    refreshing = false;
                    return;
                }
                current["content"] = languages[selectedLanguage]?.DeepClone() ?? new JsonArray();
            }
            commit();
            buildProperties();
        };
        properties.Children.Add(multilingual);
        if (current["content"] is JsonObject dictionary)
            buildLanguages(dictionary);
        else if (current["content"] is JsonArray lines)
            buildLines(properties, lines);
        refreshing = false;
    }

    private void addTimeField(JsonObject current, string field, string label)
    {
        properties.Children.Add(new TextBlock { Text = L(label) + " (s)" });
        NumericUpDown input = EditorInputs.CreateNumericUpDown(decimalTime(number(current[field])), 0, decimal.MaxValue, 0.1m);
        input.FormatString = "0.###";
        input.ValueChanged += (_, _) =>
        {
            if (refreshing)
                return;
            if (input.Value is not decimal value)
                setInputError(input, L("SUBTITLE_TIME_ERROR"));
            else
            {
                setInputError(input, null);
                current[field] = (double)value;
                commit();
            }
        };
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != NumericUpDown.TextProperty || refreshing)
                return;
            bool valid = decimal.TryParse(input.Text, NumberStyles.Number, CultureInfo.CurrentCulture, out decimal parsed) && parsed >= 0;
            setInputError(input, valid ? null : L("SUBTITLE_TIME_ERROR"));
        };
        properties.Children.Add(input);
    }

    private void buildLanguages(JsonObject languages)
    {
        string[] keys = languages.Select(pair => pair.Key).ToArray();
        if (!languages.ContainsKey(selectedLanguage))
            selectedLanguage = keys.FirstOrDefault() ?? "en_GB";
        Grid columns = new() { ColumnDefinitions = new ColumnDefinitions("130,8,*") };
        StackPanel left = new() { Spacing = 5 };
        ListBox list = new() { ItemsSource = keys, SelectedItem = selectedLanguage, MinHeight = 80 };
        list.SelectionChanged += (_, _) =>
        {
            if (refreshing)
                return;
            if (inputErrors.Count != 0)
            {
                refreshing = true;
                list.SelectedItem = selectedLanguage;
                refreshing = false;
                return;
            }
            if (list.SelectedItem is not string key)
                return;
            selectedLanguage = key;
            buildProperties();
        };
        left.Children.Add(list);
        TextBox languageKey = EditorInputs.CreateEditableTextBox(selectedLanguage);
        languageKey.TextChanged += (_, _) =>
        {
            if (refreshing)
                return;
            string next = languageKey.Text ?? string.Empty;
            if (string.IsNullOrWhiteSpace(next) || next != next.Trim() || next != selectedLanguage && languages.ContainsKey(next))
            {
                setInputError(languageKey, L("SUBTITLE_LANGUAGE_ERROR"));
                return;
            }
            setInputError(languageKey, null);
            if (next == selectedLanguage || !languages.TryGetPropertyValue(selectedLanguage, out JsonNode? lines))
                return;
            languages.Remove(selectedLanguage);
            languages[next] = lines;
            selectedLanguage = next;
            commit();
            refreshing = true;
            list.ItemsSource = languages.Select(pair => pair.Key).ToArray();
            list.SelectedItem = next;
            refreshing = false;
        };
        left.Children.Add(languageKey);
        left.Children.Add(editButton("SUBTITLE_ADD_LANGUAGE", () =>
        {
            string name = "en_GB";
            int suffix = 1;
            while (languages.ContainsKey(name))
                name = "language_" + suffix++;
            languages[name] = new JsonArray();
            selectedLanguage = name;
            commit();
            buildProperties();
        }));
        left.Children.Add(editButton("SUBTITLE_REMOVE_LANGUAGE", () =>
        {
            languages.Remove(selectedLanguage);
            commit();
            buildProperties();
        }));
        columns.Children.Add(left);
        StackPanel right = new() { Spacing = 6 };
        if (languages[selectedLanguage] is JsonArray content)
            buildLines(right, content);
        Grid.SetColumn(right, 2);
        columns.Children.Add(right);
        properties.Children.Add(columns);
    }

    private void buildLines(StackPanel panel, JsonArray lines)
    {
        for (int index = 0; index < lines.Count; index++)
        {
            int lineIndex = index;
            string text = lines[index] is JsonValue scalar && scalar.TryGetValue(out string? value)
                ? value ?? string.Empty : lines[index]?.ToJsonString() ?? string.Empty;
            TextBox input = EditorInputs.CreateEditableTextBox(text);
            input.AcceptsReturn = true;
            input.TextWrapping = TextWrapping.Wrap;
            input.MinHeight = 65;
            input.TextChanged += (_, _) =>
            {
                if (!refreshing)
                {
                    lines[lineIndex] = input.Text ?? string.Empty;
                    commit();
                }
            };
            panel.Children.Add(input);
            WrapPanel tools = new();
            Button up = editButton("SUBTITLE_UP", () => moveLine(lines, lineIndex, -1));
            up.IsEnabled = index > 0;
            Button down = editButton("SUBTITLE_DOWN", () => moveLine(lines, lineIndex, 1));
            down.IsEnabled = index + 1 < lines.Count;
            tools.Children.Add(up);
            tools.Children.Add(down);
            tools.Children.Add(editButton("SUBTITLE_REMOVE_LINE", () => { lines.RemoveAt(lineIndex); commit(); buildProperties(); }));
            panel.Children.Add(tools);
        }
        panel.Children.Add(editButton("SUBTITLE_ADD_LINE", () => { lines.Add(string.Empty); commit(); buildProperties(); }));
    }

    private void moveLine(JsonArray lines, int index, int delta)
    {
        JsonNode? item = lines[index];
        lines.RemoveAt(index);
        lines.Insert(index + delta, item);
        commit();
        buildProperties();
    }

    private void addSection()
    {
        if (inputErrors.Count != 0)
            return;
        if (data["sections"] is not JsonArray)
            data["sections"] = new JsonArray();
        double start = currentTime;
        foreach (JsonObject item in sections.OfType<JsonObject>().OrderBy(item => number(item["startTime"])))
        {
            if (number(item["endTime"]) <= start)
                continue;
            if (number(item["startTime"]) >= start + 2)
                break;
            start = number(item["endTime"]);
        }
        sections.Add(new JsonObject { ["startTime"] = start, ["endTime"] = start + 2, ["content"] = new JsonArray("") });
        commit();
        select(sections.Count - 1);
    }

    private void setInputError(Control input, string? error)
    {
        if (error is null)
            inputErrors.Remove(input);
        else
            inputErrors[input] = error;
        updateValidation();
        projectSave.NotifyPendingInputsChanged();
        binding?.Refresh();
        requestPreview("render");
    }

    private void commit()
    {
        committing = true;
        gameData.Subtitles.UpdateSubtitle(Key, data);
        committing = false;
        updateValidation();
        assetDirty = true;
        requestPreview("render");
    }

    private void updateValidation()
    {
        timeline.IsEnabled = inputErrors.Count == 0;
        errors.Text = string.Join(Environment.NewLine, inputErrors.Values.Concat(SubtitleAssetSchema.Validate(data, Key)).Distinct());
        timeline.Duration = Math.Max(videoDuration, sections.OfType<JsonObject>().Select(item => number(item["endTime"])).DefaultIfEmpty().Max());
        timeline.Refresh();
    }

    private void documentChanged(object? sender, EventArgs args)
    {
        if (committing || document.Data is not JsonObject next)
            return;
        data = next;
        inputErrors.Clear();
        projectSave.NotifyPendingInputsChanged();
        select(Math.Min(selected, sections.Count - 1));
        updateValidation();
        assetDirty = true;
        requestPreview("render");
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Avalonia.Input.Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (EditorShortcuts.IsUndo(args.Key, args.KeyModifiers))
            EditorFeedback.ShowHistory(toast, "Undo", binding.Undo());
        else if (EditorShortcuts.IsRedo(args.Key, args.KeyModifiers))
            EditorFeedback.ShowHistory(toast, "Redo", binding.Redo());
        else
            return;
        args.Handled = true;
    }
}
