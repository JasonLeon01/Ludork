using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Threading;
using System;

namespace Ludork.Controls;

internal sealed class BlueprintVariableNameLabel : TextBlock
{
    private readonly string displayName;
    private readonly string variableName;
    private readonly DispatcherTimer timer = new()
    {
        Interval = TimeSpan.FromSeconds(5),
    };

    public BlueprintVariableNameLabel(string display, string variable)
    {
        displayName = display;
        variableName = variable;
        Text = display;
        timer.Tick += (_, _) => showDisplayName();
        if (!string.Equals(displayName, variableName, StringComparison.Ordinal))
            Cursor = new Cursor(StandardCursorType.Hand);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (!args.GetCurrentPoint(this).Properties.IsLeftButtonPressed
            || string.Equals(displayName, variableName, StringComparison.Ordinal))
        {
            return;
        }
        if (string.Equals(Text, variableName, StringComparison.Ordinal))
        {
            showDisplayName();
        }
        else
        {
            Text = variableName;
            timer.Stop();
            timer.Start();
        }
        args.Handled = true;
    }

    private void showDisplayName()
    {
        timer.Stop();
        Text = displayName;
    }
}
