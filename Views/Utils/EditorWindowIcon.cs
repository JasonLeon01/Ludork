using Avalonia.Controls;
using Avalonia.Platform;
using System;
using System.IO;

namespace Ludork.Views.Utils;

public static class EditorWindowIcon
{
    private static readonly Uri IconUri = new("avares://Ludork/Assets/icon.ico");
    private static WindowIcon? shared;

    public static WindowIcon Shared
    {
        get
        {
            if (shared is not null)
                return shared;
            using Stream stream = AssetLoader.Open(IconUri);
            shared = new WindowIcon(stream);
            return shared;
        }
    }

    public static void Apply(Window window) => window.Icon = Shared;
}
