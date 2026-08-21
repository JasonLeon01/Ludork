using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using System;
using System.Runtime.CompilerServices;

namespace Ludork.Views.Utils;

public sealed class HintedTextPresenter : StackPanel
{
    private const double TextBoxHintMaxWidthRatio = 0.5;
    private const double InitialTextBoxHintMaxWidth = 320;

    public static readonly StyledProperty<string?> TextProperty =
        AvaloniaProperty.Register<HintedTextPresenter, string?>(nameof(Text));
    public static readonly StyledProperty<bool> ShowTextProperty =
        AvaloniaProperty.Register<HintedTextPresenter, bool>(nameof(ShowText), true);

    private static readonly ConditionalWeakTable<TextBox, TextBoxHintAttachment> textBoxAttachments = new();
    private readonly TextBlock textBlock;
    private readonly Border hintBorder;
    private readonly TextBlock hintBlock;

    public static IDataTemplate StringItemTemplate { get; } =
        new FuncDataTemplate<string>((text, _) => new HintedTextPresenter { Text = text });

    public HintedTextPresenter()
    {
        Orientation = Orientation.Horizontal;
        Spacing = 8;
        VerticalAlignment = VerticalAlignment.Center;
        Focusable = false;
        textBlock = new TextBlock
        {
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        hintBlock = new TextBlock
        {
            Foreground = new SolidColorBrush(Color.Parse("#888888")),
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        hintBorder = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#262626")),
            CornerRadius = new CornerRadius(3),
            Padding = new Thickness(6, 2),
            VerticalAlignment = VerticalAlignment.Center,
            Child = hintBlock,
        };
        Children.Add(textBlock);
        Children.Add(hintBorder);
        update();
    }

    public string? Text
    {
        get => GetValue(TextProperty);
        set => SetValue(TextProperty, value);
    }

    public bool ShowText
    {
        get => GetValue(ShowTextProperty);
        set => SetValue(ShowTextProperty, value);
    }

    public static void AttachTo(TextBox textBox)
    {
        if (textBoxAttachments.TryGetValue(textBox, out TextBoxHintAttachment? attachment))
        {
            attachment.Update();
            return;
        }
        TextBoxHintAttachment next = new(textBox);
        textBoxAttachments.Add(textBox, next);
    }

    public static void DetachFrom(TextBox textBox)
    {
        if (!textBoxAttachments.TryGetValue(textBox, out TextBoxHintAttachment? attachment))
            return;
        attachment.Dispose();
        textBoxAttachments.Remove(textBox);
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (change.Property == TextProperty || change.Property == ShowTextProperty)
            update();
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        TextHintService.Invalidated += onHintsInvalidated;
        update();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        TextHintService.Invalidated -= onHintsInvalidated;
        base.OnDetachedFromVisualTree(args);
    }

    private void onHintsInvalidated(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
            update();
        else
            Dispatcher.UIThread.Post(update);
    }

    private void update()
    {
        textBlock.Text = Text ?? string.Empty;
        textBlock.IsVisible = ShowText;
        IsHitTestVisible = ShowText;
        string? hint = TextHintService.Resolve(Text);
        hintBlock.Text = hint ?? string.Empty;
        hintBorder.IsVisible = !string.IsNullOrEmpty(hint);
    }

    private sealed class TextBoxHintAttachment : IDisposable
    {
        private readonly TextBox textBox;
        private readonly HintedTextPresenter presenter;

        public TextBoxHintAttachment(TextBox textBox)
        {
            this.textBox = textBox;
            presenter = new HintedTextPresenter
            {
                ShowText = false,
            };
            presenter.hintBorder.MaxWidth = InitialTextBoxHintMaxWidth;
            textBox.InnerRightContent = presenter;
            textBox.TextChanged += onTextChanged;
            textBox.SizeChanged += onSizeChanged;
            Update();
        }

        public void Update()
        {
            presenter.Text = textBox.Text;
        }

        public void Dispose()
        {
            textBox.TextChanged -= onTextChanged;
            textBox.SizeChanged -= onSizeChanged;
            if (ReferenceEquals(textBox.InnerRightContent, presenter))
                textBox.InnerRightContent = null;
        }

        private void onTextChanged(object? sender, TextChangedEventArgs args)
        {
            Update();
        }

        private void onSizeChanged(object? sender, SizeChangedEventArgs args)
        {
            double contentWidth = args.NewSize.Width
                - textBox.Padding.Left
                - textBox.Padding.Right;
            presenter.hintBorder.MaxWidth = Math.Max(
                0,
                contentWidth * TextBoxHintMaxWidthRatio);
        }
    }
}
