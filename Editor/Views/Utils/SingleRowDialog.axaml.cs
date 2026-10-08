using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Views.Utils;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public partial class SingleRowDialog : Window
{
    private readonly HashSet<string> existingNames;

    public SingleRowDialog() : this(string.Empty, string.Empty, Array.Empty<string>())
    {
    }

    public SingleRowDialog(string title, string message, IEnumerable<string> existingNames, string initialValue = "", IEnumerable<string>? suggestions = null)
    {
        this.existingNames = new HashSet<string>(existingNames, StringComparer.Ordinal);
        InitializeComponent();
        Title = title;
        MessageText.Text = message;
        EditorInputs.ApplyEditable(ValueBox);
        ValueBox.Text = initialValue;
        string[] choices = suggestions?.ToArray() ?? [];
        if (choices.Length != 0)
        {
            MenuFlyout menu = new();
            foreach (string choice in choices)
            {
                MenuItem item = new() { Header = EditorDisplayName.Format(choice) };
                item.Click += (_, _) => ValueBox.Text = choice;
                menu.Items.Add(item);
            }
            ValueBox.InnerRightContent = new Button { Content = "▾", Flyout = menu };
        }
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        Opened += (_, _) =>
        {
            ValueBox.Focus();
            ValueBox.SelectAll();
        };
    }

    public static Task<string?> ShowAsync(
        Window owner,
        string title,
        string message,
        IEnumerable<string> existingNames,
        string initialValue = "",
        IEnumerable<string>? suggestions = null
    )
    {
        return new SingleRowDialog(title, message, existingNames, initialValue, suggestions).ShowDialog<string?>(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        string value = ValueBox.Text?.Trim() ?? string.Empty;
        if (string.IsNullOrWhiteSpace(value))
        {
            ErrorText.Text = LocaleService.Get("ADD_EMPTY");
            return;
        }
        if (existingNames.Contains(value))
        {
            ErrorText.Text = LocaleService.Get("ADD_DUPLICATE");
            return;
        }
        Close(value);
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close(null);
    }
}
