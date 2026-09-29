using Avalonia.Controls;
using Ludork.Services;
using System;

namespace Ludork.Views;

public partial class DocumentationWindow : Window
{
    public DocumentationWindow()
    {
        InitializeComponent();
    }

    public DocumentationWindow(Uri uri, string title) : this()
    {
        EditorLayoutService.AttachWindow(this, nameof(DocumentationWindow));
        Title = title;
        Documentation.Navigate(uri);
    }
}
