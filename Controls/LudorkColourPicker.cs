using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Ludork.Controls;

public sealed class LudorkColourPicker : Grid
{
    private static readonly string[] basicColourHex =
    [
        "#000000", "#404040", "#808080", "#C0C0C0", "#FFFFFF", "#800000", "#FF0000", "#FF8080",
        "#808000", "#FFFF00", "#FFFF80", "#008000", "#00FF00", "#80FF80", "#008080", "#00FFFF",
        "#80FFFF", "#000080", "#0000FF", "#8080FF", "#800080", "#FF00FF", "#FF80FF", "#804000",
        "#FF8000", "#FFC080", "#804040", "#C06060", "#A0A000", "#60A060", "#408080", "#4060A0",
        "#6040A0", "#A04080", "#202020", "#606060", "#A0A0A0", "#E0E0E0", "#A00000", "#00A000",
        "#0000A0", "#A0A000", "#00A0A0", "#A000A0", "#A06000", "#0060A0", "#60A000", "#A00060",
    ];

    private static readonly List<Color?> customColours =
    [
        null, null, null, null, null, null, null, null,
        null, null, null, null, null, null, null, null,
    ];

    private readonly ColourPlaneControl colourPlane = new();
    private readonly HueBarControl hueBar = new();
    private readonly ColourPreviewControl currentPreview;
    private readonly ColourPreviewControl newPreview = new();
    private readonly Grid customPalette = createPaletteGrid();
    private readonly NumericUpDown hueBox = createNumber(0, 359);
    private readonly NumericUpDown saturationBox = createNumber(0, 255);
    private readonly NumericUpDown valueBox = createNumber(255, 255);
    private readonly NumericUpDown redBox = createNumber(255, 255);
    private readonly NumericUpDown greenBox = createNumber(255, 255);
    private readonly NumericUpDown blueBox = createNumber(255, 255);
    private readonly NumericUpDown alphaBox = createNumber(255, 255);
    private readonly TextBox htmlBox = EditorInputs.CreateEditableTextBox();
    private bool syncing;
    private int hue;
    private int saturation;
    private int value = 255;
    private int red = 255;
    private int green = 255;
    private int blue = 255;
    private int alpha = 255;

    public LudorkColourPicker(Color initial)
    {
        currentPreview = new ColourPreviewControl { Rgba = initial };
        htmlBox.MaxLength = 9;
        htmlBox.Width = 108;
        ColumnDefinitions = new ColumnDefinitions("*,181,Auto");
        ColumnSpacing = 12;
        MinWidth = 676;
        MinHeight = 360;

        colourPlane.SaturationValueChanged += (_, args) =>
        {
            saturation = args.Saturation;
            value = args.Value;
            applyHsv();
        };
        hueBar.HueChanged += (_, args) =>
        {
            hue = args.Hue;
            applyHsv();
        };

        Grid colourArea = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,28"),
            ColumnSpacing = 6,
            MinWidth = 270,
        };
        colourPlane.MinWidth = 240;
        colourPlane.MinHeight = 180;
        hueBar.Width = 28;
        hueBar.MinHeight = 180;
        colourArea.Children.Add(colourPlane);
        Grid.SetColumn(hueBar, 1);
        colourArea.Children.Add(hueBar);
        Children.Add(colourArea);

        StackPanel palettes = createPalettes();
        Grid.SetColumn(palettes, 1);
        Children.Add(palettes);

        Grid details = createDetails();
        Grid.SetColumn(details, 2);
        Children.Add(details);
        wireValueEditors();
        setRgba(initial.R, initial.G, initial.B, initial.A);
    }

    public event EventHandler<LudorkColourChangedEventArgs>? ColourChanged;
    public event EventHandler? ScreenPickRequested;

    public Color Color => Color.FromArgb((byte)alpha, (byte)red, (byte)green, (byte)blue);

    public void SetColor(Color value) => setRgba(value.R, value.G, value.B, value.A);

    public void SetScreenColour(Color sampled) => setRgba(sampled.R, sampled.G, sampled.B, alpha);

    private StackPanel createPalettes()
    {
        Grid basicPalette = createPaletteGrid();
        for (int index = 0; index < basicColourHex.Length; index++)
            addPaletteCell(basicPalette, index, createPaletteCell(Color.Parse(basicColourHex[index]), preserveAlpha: true));
        refreshCustomPalette();

        Button screenPick = new() { Content = LocaleService.Get("COLOUR_PICKER_PICK_SCREEN"), HorizontalAlignment = HorizontalAlignment.Stretch };
        screenPick.Click += (_, _) => ScreenPickRequested?.Invoke(this, EventArgs.Empty);
        Button addCustom = new() { Content = LocaleService.Get("COLOUR_PICKER_ADD_CUSTOM"), HorizontalAlignment = HorizontalAlignment.Stretch };
        addCustom.Click += (_, _) => addCustomColour();

        StackPanel panel = new() { Spacing = 8, Width = 181 };
        panel.Children.Add(new TextBlock { Text = LocaleService.Get("COLOUR_PICKER_BASIC_COLOURS"), TextWrapping = TextWrapping.Wrap });
        panel.Children.Add(basicPalette);
        panel.Children.Add(new TextBlock { Text = LocaleService.Get("COLOUR_PICKER_CUSTOM_COLOURS"), TextWrapping = TextWrapping.Wrap });
        panel.Children.Add(customPalette);
        panel.Children.Add(screenPick);
        panel.Children.Add(addCustom);
        panel.Children.Add(new Border { VerticalAlignment = VerticalAlignment.Stretch });
        return panel;
    }

    private Grid createDetails()
    {
        Grid details = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,Auto"),
            ColumnSpacing = 8,
            RowSpacing = 8,
        };
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_CURRENT"), currentPreview);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_NEW"), newPreview);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_HUE"), hueBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_SATURATION"), saturationBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_VALUE"), valueBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_RED"), redBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_GREEN"), greenBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_BLUE"), blueBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_ALPHA"), alphaBox);
        addDetailRow(details, LocaleService.Get("COLOUR_PICKER_HTML"), htmlBox);
        return details;
    }

    private void wireValueEditors()
    {
        hueBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            hue = getInt(hueBox);
            applyHsv();
        };
        saturationBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            saturation = getInt(saturationBox);
            applyHsv();
        };
        valueBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            value = getInt(valueBox);
            applyHsv();
        };
        redBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setRgba(getInt(redBox), green, blue, alpha);
        };
        greenBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setRgba(red, getInt(greenBox), blue, alpha);
        };
        blueBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setRgba(red, green, getInt(blueBox), alpha);
        };
        alphaBox.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setRgba(red, green, blue, getInt(alphaBox));
        };
        htmlBox.LostFocus += (_, _) => applyHex();
        htmlBox.KeyDown += onHtmlKeyDown;
    }

    private void onHtmlKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Enter)
            return;
        applyHex();
        args.Handled = true;
    }

    private void applyHsv()
    {
        Color rgb = hsvToColor(hue, saturation, value, (byte)alpha);
        red = rgb.R;
        green = rgb.G;
        blue = rgb.B;
        syncControls();
    }

    private void applyHex()
    {
        if (syncing)
            return;
        string text = htmlBox.Text?.Trim() ?? string.Empty;
        if (text.StartsWith("#", StringComparison.Ordinal))
            text = text[1..];
        if (text.Length is not (6 or 8)
            || !byte.TryParse(text[..2], NumberStyles.HexNumber, null, out byte parsedRed)
            || !byte.TryParse(text.Substring(2, 2), NumberStyles.HexNumber, null, out byte parsedGreen)
            || !byte.TryParse(text.Substring(4, 2), NumberStyles.HexNumber, null, out byte parsedBlue))
        {
            syncControls();
            return;
        }
        int parsedAlpha = alpha;
        if (text.Length == 8)
        {
            if (!byte.TryParse(text.Substring(6, 2), NumberStyles.HexNumber, null, out byte channel))
            {
                syncControls();
                return;
            }
            parsedAlpha = channel;
        }
        setRgba(parsedRed, parsedGreen, parsedBlue, parsedAlpha);
    }

    private void setRgba(int nextRed, int nextGreen, int nextBlue, int nextAlpha)
    {
        red = clamp(nextRed, 0, 255);
        green = clamp(nextGreen, 0, 255);
        blue = clamp(nextBlue, 0, 255);
        alpha = clamp(nextAlpha, 0, 255);
        (hue, saturation, value) = rgbToHsv(red, green, blue, hue);
        syncControls();
    }

    private void syncControls()
    {
        syncing = true;
        colourPlane.SetState(hue, saturation, value);
        hueBar.Hue = hue;
        hueBox.Value = hue;
        saturationBox.Value = saturation;
        valueBox.Value = value;
        redBox.Value = red;
        greenBox.Value = green;
        blueBox.Value = blue;
        alphaBox.Value = alpha;
        htmlBox.Text = formatHtml(red, green, blue, alpha);
        newPreview.Rgba = Color;
        syncing = false;
        ColourChanged?.Invoke(this, new LudorkColourChangedEventArgs(Color));
    }

    private void addCustomColour()
    {
        Color colour = Color;
        List<Color?> next =
        [
            colour,
            .. customColours.FindAll(item => item is Color existing && existing != colour),
        ];
        while (next.Count < 16)
            next.Add(null);
        if (next.Count > 16)
            next.RemoveRange(16, next.Count - 16);
        customColours.Clear();
        customColours.AddRange(next);
        refreshCustomPalette();
    }

    private void refreshCustomPalette()
    {
        customPalette.Children.Clear();
        for (int index = 0; index < customColours.Count; index++)
            addPaletteCell(customPalette, index, createPaletteCell(customColours[index], preserveAlpha: false));
    }

    private static Grid createPaletteGrid()
    {
        Grid grid = new()
        {
            Width = 181,
            ColumnDefinitions = new ColumnDefinitions("Auto,Auto,Auto,Auto,Auto,Auto,Auto,Auto"),
            RowSpacing = 3,
            ColumnSpacing = 3,
        };
        return grid;
    }

    private static void addPaletteCell(Grid grid, int index, Control cell)
    {
        int row = index / 8;
        int column = index % 8;
        while (grid.RowDefinitions.Count <= row)
            grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid.SetRow(cell, row);
        Grid.SetColumn(cell, column);
        grid.Children.Add(cell);
    }

    private Control createPaletteCell(Color? entry, bool preserveAlpha)
    {
        Border cell = new()
        {
            Width = 20,
            Height = 20,
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
            BorderThickness = new Thickness(1),
            ClipToBounds = true,
            Background = entry is null ? new SolidColorBrush(Color.Parse("#262626")) : Brushes.Transparent,
            Cursor = entry is null ? new Cursor(StandardCursorType.Arrow) : new Cursor(StandardCursorType.Hand),
        };
        if (entry is Color colour)
        {
            Panel content = new();
            content.Children.Add(new CheckerboardControl { CellSize = 4, Margin = new Thickness(1) });
            content.Children.Add(new Border
            {
                Margin = new Thickness(1),
                Background = new SolidColorBrush(colour),
            });
            cell.Child = content;
            cell.PointerPressed += (_, args) =>
            {
                if (!args.GetCurrentPoint(cell).Properties.IsLeftButtonPressed)
                    return;
                if (preserveAlpha)
                    setRgba(colour.R, colour.G, colour.B, alpha);
                else
                    setRgba(colour.R, colour.G, colour.B, colour.A);
                args.Handled = true;
            };
            return cell;
        }

        Panel empty = new();
        empty.Children.Add(new DiagonalMarkControl());
        cell.Child = empty;
        return cell;
    }

    private static void addDetailRow(Grid grid, string label, Control editor)
    {
        int row = grid.RowDefinitions.Count;
        grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        TextBlock labelBlock = new()
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 0),
        };
        Grid.SetRow(labelBlock, row);
        grid.Children.Add(labelBlock);
        Grid.SetRow(editor, row);
        Grid.SetColumn(editor, 1);
        grid.Children.Add(editor);
    }

    private static NumericUpDown createNumber(int value, int maximum)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(value, 0, maximum, 1, stretch: false);
        box.Width = 108;
        box.FormatString = "0";
        return box;
    }

    private static int getInt(NumericUpDown box) => clamp(decimal.ToInt32(box.Value ?? 0), 0, (int)box.Maximum);

    private static int clamp(int value, int minimum, int maximum) => Math.Max(minimum, Math.Min(maximum, value));

    private static string formatHtml(int red, int green, int blue, int alpha)
    {
        return alpha == 255
            ? $"#{red:X2}{green:X2}{blue:X2}"
            : $"#{red:X2}{green:X2}{blue:X2}{alpha:X2}";
    }

    private static (int Hue, int Saturation, int Value) rgbToHsv(int red, int green, int blue, int previousHue)
    {
        double rr = clamp(red, 0, 255) / 255.0;
        double gg = clamp(green, 0, 255) / 255.0;
        double bb = clamp(blue, 0, 255) / 255.0;
        double maximum = Math.Max(rr, Math.Max(gg, bb));
        double minimum = Math.Min(rr, Math.Min(gg, bb));
        double delta = maximum - minimum;
        double hh = previousHue;
        if (delta > 0)
        {
            if (maximum == rr)
                hh = 60 * ((gg - bb) / delta % 6);
            else if (maximum == gg)
                hh = 60 * ((bb - rr) / delta + 2);
            else
                hh = 60 * ((rr - gg) / delta + 4);
            if (hh < 0)
                hh += 360;
        }
        return ((int)Math.Round(hh), maximum == 0 ? 0 : (int)Math.Round(delta / maximum * 255), (int)Math.Round(maximum * 255));
    }

    private static Color hsvToColor(int hue, int saturation, int value, byte alpha)
    {
        double h = ((hue % 360) + 360) % 360;
        double s = clamp(saturation, 0, 255) / 255.0;
        double v = clamp(value, 0, 255) / 255.0;
        double chroma = v * s;
        double x = chroma * (1 - Math.Abs(h / 60 % 2 - 1));
        double match = v - chroma;
        (double Red, double Green, double Blue) rgb = h switch
        {
            < 60 => (chroma, x, 0),
            < 120 => (x, chroma, 0),
            < 180 => (0, chroma, x),
            < 240 => (0, x, chroma),
            < 300 => (x, 0, chroma),
            _ => (chroma, 0, x),
        };
        return Color.FromArgb(alpha, (byte)Math.Round((rgb.Red + match) * 255), (byte)Math.Round((rgb.Green + match) * 255), (byte)Math.Round((rgb.Blue + match) * 255));
    }

    private sealed class ColourPlaneControl : Control
    {
        private int hue;
        private int saturation;
        private int value = 255;
        private bool dragging;

        public event EventHandler<SaturationValueChangedEventArgs>? SaturationValueChanged;

        public ColourPlaneControl()
        {
            Cursor = new Cursor(StandardCursorType.Cross);
            ClipToBounds = true;
        }

        public void SetState(int nextHue, int nextSaturation, int nextValue)
        {
            hue = nextHue;
            saturation = nextSaturation;
            value = nextValue;
            InvalidateVisual();
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                return;
            dragging = true;
            e.Pointer.Capture(this);
            updateFromPoint(e.GetPosition(this));
            e.Handled = true;
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            if (!dragging)
                return;
            updateFromPoint(e.GetPosition(this));
            e.Handled = true;
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            if (!dragging)
                return;
            dragging = false;
            e.Pointer.Capture(null);
            e.Handled = true;
        }

        public override void Render(DrawingContext context)
        {
            Rect bounds = new(Bounds.Size);
            Rect fill = bounds.Deflate(1);
            Color hueColor = hsvToColor(hue, 255, 255, 255);
            context.FillRectangle(
                new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(1, 0, RelativeUnit.Relative),
                    GradientStops = { new GradientStop(Colors.White, 0), new GradientStop(hueColor, 1) },
                },
                fill);
            context.FillRectangle(
                new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
                    GradientStops = { new GradientStop(Colors.Transparent, 0), new GradientStop(Colors.Black, 1) },
                },
                fill);
            context.DrawRectangle(new Pen(new SolidColorBrush(Color.Parse("#373737")), 1), bounds);

            double indicatorX = 1 + saturation * Math.Max(1, bounds.Width - 3) / 255.0;
            double indicatorY = 1 + (255 - value) * Math.Max(1, bounds.Height - 3) / 255.0;
            Rect outer = new(indicatorX - 6.5, indicatorY - 6.5, 13, 13);
            context.DrawEllipse(null, new Pen(Brushes.Black, 3), outer.Center, 6.5, 6.5);
            context.DrawEllipse(null, new Pen(Brushes.White, 1), outer.Center, 4.5, 4.5);
        }

        private void updateFromPoint(Point point)
        {
            double width = Math.Max(1, Bounds.Width - 3);
            double height = Math.Max(1, Bounds.Height - 3);
            int nextSaturation = clamp((int)Math.Round((point.X - 1) * 255 / width), 0, 255);
            int nextValue = 255 - clamp((int)Math.Round((point.Y - 1) * 255 / height), 0, 255);
            if (nextSaturation == saturation && nextValue == value)
                return;
            saturation = nextSaturation;
            value = nextValue;
            InvalidateVisual();
            SaturationValueChanged?.Invoke(this, new SaturationValueChangedEventArgs(saturation, value));
        }
    }

    private sealed class HueBarControl : Control
    {
        private int hue;
        private bool dragging;

        public event EventHandler<HueChangedEventArgs>? HueChanged;

        public HueBarControl()
        {
            Cursor = new Cursor(StandardCursorType.Hand);
        }

        public int Hue
        {
            get => hue;
            set
            {
                if (hue == value)
                    return;
                hue = value;
                InvalidateVisual();
            }
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                return;
            dragging = true;
            e.Pointer.Capture(this);
            updateFromPoint(e.GetPosition(this));
            e.Handled = true;
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            if (!dragging)
                return;
            updateFromPoint(e.GetPosition(this));
            e.Handled = true;
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            if (!dragging)
                return;
            dragging = false;
            e.Pointer.Capture(null);
            e.Handled = true;
        }

        public override void Render(DrawingContext context)
        {
            Rect strip = new(7, 1, 13, Math.Max(1, Bounds.Height - 3));
            context.FillRectangle(
                new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
                    GradientStops =
                    {
                        new GradientStop(Color.Parse("#ff0000"), 0),
                        new GradientStop(Color.Parse("#ffff00"), 0.1667),
                        new GradientStop(Color.Parse("#00ff00"), 0.3333),
                        new GradientStop(Color.Parse("#00ffff"), 0.5),
                        new GradientStop(Color.Parse("#0000ff"), 0.6667),
                        new GradientStop(Color.Parse("#ff00ff"), 0.8333),
                        new GradientStop(Color.Parse("#ff0000"), 1),
                    },
                },
                strip);
            context.DrawRectangle(new Pen(new SolidColorBrush(Color.Parse("#373737")), 1), strip);

            double indicatorY = 1 + hue * Math.Max(1, Bounds.Height - 4) / 359.0 - 1;
            Rect indicator = new(2, indicatorY, Math.Max(1, Bounds.Width - 4), 3);
            context.FillRectangle(Brushes.White, indicator);
            context.DrawRectangle(new Pen(Brushes.Black, 1), indicator);
        }

        private void updateFromPoint(Point point)
        {
            int nextHue = clamp((int)Math.Round((point.Y - 1) * 359 / Math.Max(1, Bounds.Height - 4)), 0, 359);
            if (nextHue == hue)
                return;
            hue = nextHue;
            InvalidateVisual();
            HueChanged?.Invoke(this, new HueChangedEventArgs(hue));
        }
    }

    private sealed class ColourPreviewControl : Border
    {
        private Color rgba = Colors.White;

        public ColourPreviewControl()
        {
            Width = 76;
            Height = 32;
            ClipToBounds = true;
            BorderBrush = new SolidColorBrush(Color.Parse("#373737"));
            BorderThickness = new Thickness(1);
            Child = new Panel
            {
                Children =
                {
                    new CheckerboardControl { CellSize = 5, Margin = new Thickness(1) },
                    new Border { Name = "Fill", Margin = new Thickness(1) },
                },
            };
        }

        public Color Rgba
        {
            get => rgba;
            set
            {
                rgba = value;
                if (Child is Panel panel && panel.Children[1] is Border fill)
                    fill.Background = new SolidColorBrush(value);
            }
        }
    }

    private sealed class CheckerboardControl : Control
    {
        public static readonly StyledProperty<int> CellSizeProperty =
            AvaloniaProperty.Register<CheckerboardControl, int>(nameof(CellSize), 5);

        public int CellSize
        {
            get => GetValue(CellSizeProperty);
            set => SetValue(CellSizeProperty, value);
        }

        public override void Render(DrawingContext context)
        {
            int cell = Math.Max(1, CellSize);
            int columns = Math.Max(1, (int)Math.Ceiling(Bounds.Width / cell));
            int rows = Math.Max(1, (int)Math.Ceiling(Bounds.Height / cell));
            IBrush light = new SolidColorBrush(Color.Parse("#d2d2d2"));
            IBrush dark = new SolidColorBrush(Color.Parse("#969696"));
            for (int row = 0; row < rows; row++)
            {
                for (int column = 0; column < columns; column++)
                {
                    IBrush brush = ((column + row) & 1) == 0 ? light : dark;
                    context.FillRectangle(brush, new Rect(column * cell, row * cell, cell, cell));
                }
            }
        }
    }

    private sealed class DiagonalMarkControl : Control
    {
        public override void Render(DrawingContext context)
        {
            double length = Math.Sqrt(Bounds.Width * Bounds.Width + Bounds.Height * Bounds.Height);
            Point center = new(Bounds.Width / 2, Bounds.Height / 2);
            double angle = -Math.PI / 4;
            Vector direction = new(Math.Cos(angle), Math.Sin(angle));
            Point start = center - direction * (length / 2);
            Point end = center + direction * (length / 2);
            context.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#525252")), 1), start, end);
        }
    }

    private sealed class SaturationValueChangedEventArgs(int saturation, int value) : EventArgs
    {
        public int Saturation { get; } = saturation;
        public int Value { get; } = value;
    }

    private sealed class HueChangedEventArgs(int hue) : EventArgs
    {
        public int Hue { get; } = hue;
    }
}

public sealed class LudorkColourChangedEventArgs(Color color) : EventArgs
{
    public Color Color { get; } = color;
}
