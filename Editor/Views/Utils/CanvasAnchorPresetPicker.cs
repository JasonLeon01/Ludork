using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Collections.Generic;

namespace Ludork.Views.Utils;

public readonly record struct CanvasAnchorPreset(
    string LocaleKey,
    double MinimumX,
    double MinimumY,
    double MaximumX,
    double MaximumY,
    double AlignmentX,
    double AlignmentY);

public sealed class CanvasAnchorPresetPicker : Button
{
    private static readonly IReadOnlyList<CanvasAnchorPreset> Presets =
    [
        new("ANCHOR_TOP_LEFT", 0, 0, 0, 0, 0, 0),
        new("ANCHOR_TOP_CENTER", 0.5, 0, 0.5, 0, 0.5, 0),
        new("ANCHOR_TOP_RIGHT", 1, 0, 1, 0, 1, 0),
        new("ANCHOR_STRETCH_HORIZONTAL_TOP", 0, 0, 1, 0, 0, 0),
        new("ANCHOR_CENTER_LEFT", 0, 0.5, 0, 0.5, 0, 0.5),
        new("ANCHOR_CENTER", 0.5, 0.5, 0.5, 0.5, 0.5, 0.5),
        new("ANCHOR_CENTER_RIGHT", 1, 0.5, 1, 0.5, 1, 0.5),
        new("ANCHOR_STRETCH_HORIZONTAL_CENTER", 0, 0.5, 1, 0.5, 0, 0.5),
        new("ANCHOR_BOTTOM_LEFT", 0, 1, 0, 1, 0, 1),
        new("ANCHOR_BOTTOM_CENTER", 0.5, 1, 0.5, 1, 0.5, 1),
        new("ANCHOR_BOTTOM_RIGHT", 1, 1, 1, 1, 1, 1),
        new("ANCHOR_STRETCH_HORIZONTAL_BOTTOM", 0, 1, 1, 1, 0, 1),
        new("ANCHOR_STRETCH_VERTICAL_LEFT", 0, 0, 0, 1, 0, 0),
        new("ANCHOR_STRETCH_VERTICAL_CENTER", 0.5, 0, 0.5, 1, 0.5, 0),
        new("ANCHOR_STRETCH_VERTICAL_RIGHT", 1, 0, 1, 1, 1, 0),
        new("ANCHOR_STRETCH_ALL", 0, 0, 1, 1, 0, 0),
    ];

    private static readonly IBrush InputBackground =
        new SolidColorBrush(Color.Parse("#333333"));
    private static readonly IBrush PopupBackground =
        new SolidColorBrush(Color.Parse("#202225"));
    private static readonly IBrush CellBackground =
        new SolidColorBrush(Color.Parse("#292B2E"));
    private static readonly IBrush FieldBorder =
        new SolidColorBrush(EditorInputs.FieldBorderColor);
    private static readonly IBrush ActiveBorder =
        new SolidColorBrush(Color.Parse("#2B82C9"));

    public CanvasAnchorPresetPicker(
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        Height = EditorInputs.FieldMinHeight;
        MinHeight = EditorInputs.FieldMinHeight;
        Padding = new Thickness(6, 2);
        HorizontalAlignment = HorizontalAlignment.Stretch;
        HorizontalContentAlignment = HorizontalAlignment.Stretch;
        Background = InputBackground;
        BorderBrush = FieldBorder;
        BorderThickness = new Thickness(1);
        CornerRadius = new CornerRadius(4);
        Content = createButtonContent(minimumX, minimumY, maximumX, maximumY);

        Flyout presetFlyout = new()
        {
            Placement = PlacementMode.BottomEdgeAlignedLeft,
        };
        presetFlyout.Content = createPresetGrid(
            presetFlyout,
            minimumX,
            minimumY,
            maximumX,
            maximumY);
        Flyout = presetFlyout;
    }

    public event Action<CanvasAnchorPreset>? PresetSelected;

    private static Control createButtonContent(
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        Grid content = new()
        {
            ColumnDefinitions = new ColumnDefinitions("28,*,Auto"),
            ColumnSpacing = 7,
        };
        CanvasAnchorPresetIcon icon = new(
            minimumX,
            minimumY,
            maximumX,
            maximumY)
        {
            Width = 24,
            Height = 24,
        };
        TextBlock label = new()
        {
            Text = LocaleService.Get("ANCHORS"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        TextBlock arrow = new()
        {
            Text = "▾",
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = Brushes.Gray,
        };
        Grid.SetColumn(label, 1);
        Grid.SetColumn(arrow, 2);
        content.Children.Add(icon);
        content.Children.Add(label);
        content.Children.Add(arrow);
        return content;
    }

    private Control createPresetGrid(
        Flyout flyout,
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        Grid grid = new()
        {
            Width = 304,
            Height = 304,
            RowDefinitions = new RowDefinitions("*,*,*,*"),
            ColumnDefinitions = new ColumnDefinitions("*,*,*,*"),
        };
        for (int index = 0; index < Presets.Count; index++)
        {
            CanvasAnchorPreset preset = Presets[index];
            Button button = new()
            {
                Margin = new Thickness(1),
                Padding = new Thickness(5),
                Background = CellBackground,
                BorderBrush = matches(
                    preset,
                    minimumX,
                    minimumY,
                    maximumX,
                    maximumY)
                    ? ActiveBorder
                    : FieldBorder,
                BorderThickness = matches(
                    preset,
                    minimumX,
                    minimumY,
                    maximumX,
                    maximumY)
                    ? new Thickness(2)
                    : new Thickness(1),
                CornerRadius = new CornerRadius(0),
                HorizontalContentAlignment = HorizontalAlignment.Stretch,
                VerticalContentAlignment = VerticalAlignment.Stretch,
                Content = new CanvasAnchorPresetIcon(
                    preset.MinimumX,
                    preset.MinimumY,
                    preset.MaximumX,
                    preset.MaximumY),
            };
            ToolTip.SetTip(button, LocaleService.Get(preset.LocaleKey));
            button.Click += (_, _) =>
            {
                flyout.Hide();
                PresetSelected?.Invoke(preset);
            };
            Grid.SetRow(button, index / 4);
            Grid.SetColumn(button, index % 4);
            grid.Children.Add(button);
        }

        Border verticalSeparator = new()
        {
            BorderBrush = ActiveBorder,
            BorderThickness = new Thickness(1, 0, 0, 0),
            IsHitTestVisible = false,
        };
        Grid.SetColumn(verticalSeparator, 3);
        Grid.SetRowSpan(verticalSeparator, 4);
        grid.Children.Add(verticalSeparator);

        Border horizontalSeparator = new()
        {
            BorderBrush = ActiveBorder,
            BorderThickness = new Thickness(0, 1, 0, 0),
            IsHitTestVisible = false,
        };
        Grid.SetRow(horizontalSeparator, 3);
        Grid.SetColumnSpan(horizontalSeparator, 4);
        grid.Children.Add(horizontalSeparator);

        return new Border
        {
            Padding = new Thickness(4),
            Background = PopupBackground,
            BorderBrush = FieldBorder,
            BorderThickness = new Thickness(1),
            Child = grid,
        };
    }

    private static bool matches(
        CanvasAnchorPreset preset,
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        return close(preset.MinimumX, minimumX)
            && close(preset.MinimumY, minimumY)
            && close(preset.MaximumX, maximumX)
            && close(preset.MaximumY, maximumY);
    }

    private static bool close(double left, double right)
    {
        return Math.Abs(left - right) < 0.0001;
    }
}

internal sealed class CanvasAnchorPresetIcon : Control
{
    private static readonly IBrush BackgroundBrush =
        new SolidColorBrush(Color.Parse("#202225"));
    private static readonly IBrush GridBrush =
        new SolidColorBrush(Color.Parse("#34383D"));
    private static readonly IBrush FrameBrush =
        new SolidColorBrush(Color.Parse("#686C70"));
    private static readonly IBrush AnchorBrush =
        new SolidColorBrush(Color.Parse("#D8D8D8"));
    private static readonly IBrush AnchorBorderBrush =
        new SolidColorBrush(Color.Parse("#8B8E91"));
    private readonly double minimumX;
    private readonly double minimumY;
    private readonly double maximumX;
    private readonly double maximumY;

    public CanvasAnchorPresetIcon(
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        this.minimumX = minimumX;
        this.minimumY = minimumY;
        this.maximumX = maximumX;
        this.maximumY = maximumY;
        MinWidth = 20;
        MinHeight = 20;
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(0, 0, Bounds.Width, Bounds.Height);
        context.DrawRectangle(BackgroundBrush, null, bounds);
        if (Bounds.Width < 8 || Bounds.Height < 8)
            return;

        Pen gridPen = new(GridBrush, 1);
        for (int index = 1; index < 4; index++)
        {
            double x = Bounds.Width * index / 4;
            double y = Bounds.Height * index / 4;
            context.DrawLine(gridPen, new Point(x, 0), new Point(x, Bounds.Height));
            context.DrawLine(gridPen, new Point(0, y), new Point(Bounds.Width, y));
        }

        double inset = Math.Max(4, Math.Min(Bounds.Width, Bounds.Height) * 0.16);
        Rect frame = new(
            inset,
            inset,
            Math.Max(1, Bounds.Width - inset * 2),
            Math.Max(1, Bounds.Height - inset * 2));
        context.DrawRectangle(null, new Pen(FrameBrush, 1), frame);

        bool stretchesX = !close(minimumX, maximumX);
        bool stretchesY = !close(minimumY, maximumY);
        double anchorWidth = stretchesX
            ? Math.Max(5, frame.Width * (maximumX - minimumX))
            : Math.Max(5, Math.Min(14, Bounds.Width * 0.24));
        double anchorHeight = stretchesY
            ? Math.Max(5, frame.Height * (maximumY - minimumY))
            : Math.Max(5, Math.Min(14, Bounds.Height * 0.24));
        double centerX = frame.Left + frame.Width * ((minimumX + maximumX) / 2);
        double centerY = frame.Top + frame.Height * ((minimumY + maximumY) / 2);
        Rect anchor = new(
            Math.Clamp(centerX - anchorWidth / 2, 1, Bounds.Width - anchorWidth - 1),
            Math.Clamp(centerY - anchorHeight / 2, 1, Bounds.Height - anchorHeight - 1),
            anchorWidth,
            anchorHeight);
        context.DrawRectangle(
            AnchorBrush,
            new Pen(AnchorBorderBrush, 1),
            anchor);
    }

    private static bool close(double left, double right)
    {
        return Math.Abs(left - right) < 0.0001;
    }
}
