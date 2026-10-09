using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;

namespace Ludork.Views;

public sealed class EditorOperationProgressWindow : Window
{
    private readonly TextBlock stage = new();
    private readonly TextBlock resource = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock count = new();
    private readonly ProgressBar progress = new() { Height = 8, IsIndeterminate = true };
    private bool finished;

    public EditorOperationProgressWindow(string title)
    {
        Title = title;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        Width = 420;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        ShowInTaskbar = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Content = new StackPanel
        {
            Margin = new Thickness(24), Spacing = 12,
            Children = { stage, resource, progress, count },
        };
        Closing += (_, args) => args.Cancel = !finished;
    }

    public void Update(EditorOperationProgress value)
    {
        stage.Text = LocaleService.Get(value.Stage);
        resource.Text = value.Resource ?? string.Empty;
        resource.IsVisible = !string.IsNullOrWhiteSpace(value.Resource);
        progress.IsIndeterminate = value.Total is not > 0;
        progress.Maximum = value.Total is > 0 ? value.Total.Value : 1;
        progress.Value = value.Completed;
        count.Text = value.Total is > 0 ? $"{value.Completed} / {value.Total}" : string.Empty;
        count.IsVisible = value.Total is > 0;
    }

    public void Finish()
    {
        finished = true;
        Close();
    }
}
