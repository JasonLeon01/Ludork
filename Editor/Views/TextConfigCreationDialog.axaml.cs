using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using Ludork.Views.Utils;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class TextConfigCreationDialog : Window
{
    private readonly string root;
    private readonly string initialDirectory;

    public TextConfigCreationDialog()
        : this(Directory.GetCurrentDirectory(), null)
    {
    }

    public TextConfigCreationDialog(string root, string? initialDirectory)
    {
        InitializeComponent();
        this.root = Path.GetFullPath(root);
        this.initialDirectory = string.IsNullOrWhiteSpace(initialDirectory)
            ? this.root
            : Path.GetFullPath(initialDirectory);
        EditorInputs.ApplyReadOnly(PathBox);
        Title = LocaleService.Get("NEW_TEXT_CONFIG");
        TypeLabel.Text = LocaleService.Get("TEXT_CONFIG_TYPE");
        PlainOption.Content = LocaleService.Get("TEXT_CONFIG_TYPE_PLAIN");
        RichOption.Content = LocaleService.Get("TEXT_CONFIG_TYPE_RICH");
        PathLabel.Text = LocaleService.Get("TEXT_CONFIG_PATH");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        ConfirmButton.IsEnabled = false;
        Opened += (_, _) => PlainOption.Focus();
    }

    public static Task<TextConfigCreationResult?> ShowAsync(
        Window owner,
        string root,
        string? initialDirectory = null)
    {
        TextConfigCreationDialog dialog = new(root, initialDirectory);
        return dialog.ShowDialog<TextConfigCreationResult?>(owner);
    }

    private async void onBrowse(object? sender, RoutedEventArgs args)
    {
        string startDirectory = initialDirectory;
        if (!string.IsNullOrWhiteSpace(PathBox.Text))
            startDirectory = Path.GetDirectoryName(PathBox.Text) ?? initialDirectory;
        string? selectedPath = await FileSelectorDialog.ShowAsync(
            this,
            root,
            FileSelectorDialog.FilesFilter("*.json"),
            LocaleService.Get("SELECT_TEXT_CONFIG_PATH"),
            save: true,
            initialDirectory: startDirectory);
        if (selectedPath is null)
            return;
        PathBox.Text = selectedPath;
        ConfirmButton.IsEnabled = true;
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (string.IsNullOrWhiteSpace(PathBox.Text))
            return;
        string type = RichOption.IsChecked == true
            ? "richTextConfig"
            : "plainTextConfig";
        Close(new TextConfigCreationResult(type, PathBox.Text));
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((TextConfigCreationResult?)null);
    }
}

public sealed record TextConfigCreationResult(string Type, string Path);
