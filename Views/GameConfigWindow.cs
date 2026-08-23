using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class GameConfigWindow : Window
{
    private static readonly double[] scaleValues = [0.0, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0];
    private static readonly double[] maximumRenderScaleValues = [0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 0.0];
    private static readonly double[] lightingRenderScaleValues = [0.5, 0.75, 1.0];
    private static readonly int[] frameRateValues = [30, 60, 90, 120];
    private static readonly int[] antiAliasingLevelValues = [0, 2, 4, 8];
    private readonly GameConfigData initialData;
    private readonly ComboBox languageBox;
    private readonly ComboBox scaleBox;
    private readonly double[] maximumRenderScaleOptions;
    private readonly ComboBox maximumRenderScaleBox;
    private readonly ComboBox lightingRenderScaleBox;
    private readonly ComboBox frameRateBox;
    private readonly ComboBox antiAliasingLevelBox;
    private readonly CheckBox verticalSyncBox;
    private readonly CheckBox musicOnBox;
    private readonly CheckBox soundOnBox;
    private readonly CheckBox voiceOnBox;
    private readonly NumericUpDown musicVolumeBox;
    private readonly NumericUpDown soundVolumeBox;
    private readonly NumericUpDown voiceVolumeBox;
    private readonly Button confirmButton;

    private GameConfigWindow(GameConfigData initialData, IReadOnlyList<string> languages)
    {
        Title = LocaleService.Get("GAME_CONFIG");
        Width = 560;
        Height = 616;
        MinWidth = 560;
        MinHeight = 320;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        TextBox scriptBox = EditorInputs.CreateReadOnlyTextBox(initialData.Script);
        languageBox = createComboBox(languages.Cast<object>().ToArray());
        languageBox.SelectedItem = languages.FirstOrDefault(
            language => language.Equals(initialData.Language, StringComparison.Ordinal));

        string[] scales = scaleValues
            .Select(value => value == 0.0
                ? LocaleService.Get("FULLSCREEN")
                : value.ToString("F2", CultureInfo.InvariantCulture))
            .ToArray();
        scaleBox = createComboBox(scales.Cast<object>().ToArray());
        double selectedScale = scaleValues.Contains(Math.Round(initialData.Scale, 2))
            ? Math.Round(initialData.Scale, 2)
            : 1.0;
        scaleBox.SelectedIndex = Array.IndexOf(scaleValues, selectedScale);

        maximumRenderScaleOptions = getMaximumRenderScaleOptions(initialData.MaximumRenderScale);
        string[] maximumRenderScales = maximumRenderScaleOptions
            .Select(value => value == 0.0
                ? LocaleService.Get("UNLIMITED")
                : value.ToString("G8", CultureInfo.InvariantCulture))
            .ToArray();
        maximumRenderScaleBox = createComboBox(maximumRenderScales.Cast<object>().ToArray());
        maximumRenderScaleBox.SelectedIndex = Array.IndexOf(
            maximumRenderScaleOptions,
            initialData.MaximumRenderScale);

        string[] lightingRenderScales = lightingRenderScaleValues
            .Select(value => value.ToString("F2", CultureInfo.InvariantCulture))
            .ToArray();
        lightingRenderScaleBox = createComboBox(lightingRenderScales.Cast<object>().ToArray());
        lightingRenderScaleBox.SelectedIndex = Array.IndexOf(
            lightingRenderScaleValues,
            initialData.LightingRenderScale);

        string[] frameRates = frameRateValues
            .Select(value => value.ToString(CultureInfo.InvariantCulture))
            .ToArray();
        frameRateBox = createComboBox(frameRates.Cast<object>().ToArray());
        int selectedFrameRate = frameRateValues.Contains(initialData.FrameRate)
            ? initialData.FrameRate
            : frameRateValues.MinBy(value => Math.Abs(value - initialData.FrameRate));
        frameRateBox.SelectedItem = selectedFrameRate.ToString(CultureInfo.InvariantCulture);
        string[] antiAliasingLevels = antiAliasingLevelValues
            .Append(initialData.AntiAliasingLevel)
            .Distinct()
            .Order()
            .Select(value => value.ToString(CultureInfo.InvariantCulture))
            .ToArray();
        antiAliasingLevelBox = createComboBox(
            antiAliasingLevels.Cast<object>().ToArray());
        antiAliasingLevelBox.SelectedItem =
            initialData.AntiAliasingLevel.ToString(CultureInfo.InvariantCulture);
        this.initialData = initialData with
        {
            Scale = selectedScale,
            FrameRate = selectedFrameRate,
        };

        verticalSyncBox = new CheckBox { IsChecked = initialData.VerticalSync };
        musicOnBox = new CheckBox { IsChecked = initialData.MusicOn };
        soundOnBox = new CheckBox { IsChecked = initialData.SoundOn };
        voiceOnBox = new CheckBox { IsChecked = initialData.VoiceOn };
        musicVolumeBox = createVolumeBox(initialData.MusicVolume);
        soundVolumeBox = createVolumeBox(initialData.SoundVolume);
        voiceVolumeBox = createVolumeBox(initialData.VoiceVolume);

        Grid form = new() { RowSpacing = 8 };
        addRow(form, LocaleService.Get("script"), scriptBox);
        addRow(form, LocaleService.Get("language"), languageBox);
        addRow(form, LocaleService.Get("scale"), scaleBox);
        addRow(form, LocaleService.Get("maxrenderscale"), maximumRenderScaleBox);
        addRow(form, LocaleService.Get("lightingrenderscale"), lightingRenderScaleBox);
        addRow(form, LocaleService.Get("framerate"), frameRateBox);
        addRow(form, LocaleService.Get("antialiasinglevel"), antiAliasingLevelBox);
        addRow(form, LocaleService.Get("verticalsync"), verticalSyncBox);
        addRow(form, LocaleService.Get("musicon"), musicOnBox);
        addRow(form, LocaleService.Get("soundon"), soundOnBox);
        addRow(form, LocaleService.Get("voiceon"), voiceOnBox);
        addRow(form, LocaleService.Get("musicvolume"), musicVolumeBox);
        addRow(form, LocaleService.Get("soundvolume"), soundVolumeBox);
        addRow(form, LocaleService.Get("voicevolume"), voiceVolumeBox);

        confirmButton = new Button
        {
            Content = LocaleService.Get("CONFIRM"),
            MinWidth = 80,
        };
        confirmButton.Click += (_, _) => confirm();
        Button cancelButton = new()
        {
            Content = LocaleService.Get("CANCEL"),
            MinWidth = 80,
        };
        cancelButton.Click += (_, _) => Close(null);
        languageBox.SelectionChanged += (_, _) => updateConfirmEnabled();
        updateConfirmEnabled();

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirmButton, cancelButton },
        };
        DockPanel content = new() { Margin = new Thickness(20) };
        DockPanel.SetDock(buttons, Dock.Bottom);
        content.Children.Add(buttons);
        content.Children.Add(new ScrollViewer
        {
            Content = form,
            Margin = new Thickness(0, 0, 0, 12),
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
        });
        Content = content;
        Opened += (_, _) => languageBox.Focus();
    }

    public static async Task<GameConfigData?> ShowAsync(Window owner, GameConfigService service)
    {
        if (service.LoadError is string loadError)
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("ERROR"),
                LocaleService.Get("GAME_CONFIG_LOAD_FAILED") + Environment.NewLine + loadError);
        }
        GameConfigWindow window = new(service.CurrentData, service.GetLanguageOptions());
        GameConfigData? result = await window.ShowDialog<GameConfigData?>(owner);
        if (result is not null)
            service.SetPending(result, window.initialData);
        return result;
    }

    private void confirm()
    {
        string language = languageBox.SelectedItem?.ToString()?.Trim() ?? string.Empty;
        if (language.Length == 0)
            return;
        double scale = scaleBox.SelectedIndex >= 0
            ? scaleValues[scaleBox.SelectedIndex]
            : 1.0;
        double maximumRenderScale = maximumRenderScaleBox.SelectedIndex >= 0
            ? maximumRenderScaleOptions[maximumRenderScaleBox.SelectedIndex]
            : 2.0;
        double lightingRenderScale = lightingRenderScaleBox.SelectedIndex >= 0
            ? lightingRenderScaleValues[lightingRenderScaleBox.SelectedIndex]
            : 1.0;
        int frameRate = int.Parse(
            frameRateBox.SelectedItem?.ToString() ?? "30",
            NumberStyles.Integer,
            CultureInfo.InvariantCulture);
        int antiAliasingLevel = int.Parse(
            antiAliasingLevelBox.SelectedItem?.ToString() ?? "0",
            NumberStyles.Integer,
            CultureInfo.InvariantCulture);
        Close(initialData with
        {
            Language = language,
            Scale = scale,
            MaximumRenderScale = maximumRenderScale,
            LightingRenderScale = lightingRenderScale,
            FrameRate = frameRate,
            AntiAliasingLevel = antiAliasingLevel,
            VerticalSync = verticalSyncBox.IsChecked == true,
            MusicOn = musicOnBox.IsChecked == true,
            SoundOn = soundOnBox.IsChecked == true,
            VoiceOn = voiceOnBox.IsChecked == true,
            MusicVolume = decimal.ToDouble(musicVolumeBox.Value ?? 100),
            SoundVolume = decimal.ToDouble(soundVolumeBox.Value ?? 100),
            VoiceVolume = decimal.ToDouble(voiceVolumeBox.Value ?? 100),
        });
    }

    private void updateConfirmEnabled()
    {
        confirmButton.IsEnabled = !string.IsNullOrWhiteSpace(languageBox.SelectedItem?.ToString());
    }

    private static ComboBox createComboBox(IReadOnlyList<object> items)
    {
        return new ComboBox
        {
            ItemsSource = items,
            MinHeight = EditorInputs.FieldMinHeight,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
    }

    private static double[] getMaximumRenderScaleOptions(double configuredScale)
    {
        List<double> values = maximumRenderScaleValues
            .Where(value => value > 0.0)
            .ToList();
        if (configuredScale > 0.0 && !values.Contains(configuredScale))
        {
            values.Add(configuredScale);
            values.Sort();
        }
        values.Add(0.0);
        return values.ToArray();
    }

    private static NumericUpDown createVolumeBox(double value)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            (decimal)Math.Clamp(value, 0.0, 100.0),
            0,
            100,
            1);
        box.FormatString = "0.00";
        return box;
    }

    private static void addRow(Grid form, string label, Control editor)
    {
        int rowIndex = form.RowDefinitions.Count;
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("160,*"),
            ColumnSpacing = 12,
        };
        row.Children.Add(new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
        });
        Grid.SetColumn(editor, 1);
        row.Children.Add(editor);
        Grid.SetRow(row, rowIndex);
        form.Children.Add(row);
    }
}
