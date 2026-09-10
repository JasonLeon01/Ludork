using Avalonia.Media.Imaging;
using System.IO;

namespace Ludork.Views;

public partial class FilePreviewDialog : Avalonia.Controls.Window
{
    public FilePreviewDialog()
    {
        InitializeComponent();
    }

    public FilePreviewDialog(string path)
    {
        InitializeComponent();
        Title = Path.GetFileName(path);
        PreviewImage.Source = new Bitmap(path);
    }
}
