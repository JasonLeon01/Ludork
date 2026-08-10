using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class BlueprintValidationDialog : Window
{
    private readonly bool allowContinue;

    public BlueprintValidationDialog(
        IReadOnlyList<BlueprintValidationResult> results,
        bool allowContinue)
    {
        this.allowContinue = allowContinue;
        Title = LocaleService.Get("BLUEPRINT_VALIDATION_TITLE");
        Width = 720;
        Height = 480;
        MinWidth = 520;
        MinHeight = 320;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#121212"));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        SelectableTextBlock details = new()
        {
            Text = formatResults(results),
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(16),
        };
        Border detailsHost = new()
        {
            Background = new SolidColorBrush(Color.Parse("#202124")),
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            Child = new ScrollViewer
            {
                HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
                VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
                Content = details,
            },
        };
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        if (allowContinue)
        {
            Button continueButton = new() { Content = LocaleService.Get("SAVE_ANYWAY") };
            continueButton.Click += (_, _) => Close(true);
            buttons.Children.Add(continueButton);
            Button cancelButton = new() { Content = LocaleService.Get("CANCEL") };
            cancelButton.Click += (_, _) => Close(false);
            buttons.Children.Add(cancelButton);
        }
        else
        {
            Button confirmButton = new() { Content = LocaleService.Get("CONFIRM") };
            confirmButton.Click += (_, _) => Close();
            buttons.Children.Add(confirmButton);
        }

        Grid root = new()
        {
            Margin = new Thickness(20),
            RowDefinitions = new RowDefinitions("Auto,12,*,16,Auto"),
        };
        TextBlock heading = new()
        {
            Text = allowContinue
                ? LocaleService.Get("BLUEPRINT_VALIDATION_SAVE_WARNING")
                : LocaleService.Get("BLUEPRINT_VALIDATION_TITLE"),
            TextWrapping = TextWrapping.Wrap,
            FontSize = 16,
        };
        root.Children.Add(heading);
        Grid.SetRow(detailsHost, 2);
        root.Children.Add(detailsHost);
        Grid.SetRow(buttons, 4);
        root.Children.Add(buttons);
        Content = root;
        KeyDown += onKeyDown;
    }

    public static Task ShowResultsAsync(
        Window owner,
        IReadOnlyList<BlueprintValidationResult> results)
    {
        return new BlueprintValidationDialog(results, false).ShowDialog(owner);
    }

    public static Task<bool> ShowSaveConfirmationAsync(
        Window owner,
        IReadOnlyList<BlueprintValidationResult> results)
    {
        return new BlueprintValidationDialog(results, true).ShowDialog<bool>(owner);
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        if (allowContinue)
            Close(false);
        else
            Close();
        args.Handled = true;
    }

    private static string formatResults(IReadOnlyList<BlueprintValidationResult> results)
    {
        BlueprintValidationResult[] failures = results
            .Where(result => !result.IsValid)
            .OrderBy(result => result.BlueprintKey, StringComparer.Ordinal)
            .ToArray();
        if (failures.Length == 0)
            return LocaleService.Get("BLUEPRINT_VALIDATION_SUCCESS");
        StringBuilder text = new();
        for (int resultIndex = 0; resultIndex < failures.Length; resultIndex++)
        {
            BlueprintValidationResult result = failures[resultIndex];
            if (resultIndex != 0)
                text.AppendLine();
            text.AppendLine(result.BlueprintKey);
            foreach (string error in result.Errors)
                text.Append("  • ").AppendLine(error);
        }
        return text.ToString().TrimEnd();
    }
}
