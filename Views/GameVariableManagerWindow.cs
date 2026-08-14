using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class GameVariableManagerWindow : Window
{
    private static readonly GameVariableTypeOption[] TypeOptions =
        Enum.GetValues<GameVariableType>()
            .Select(type => new GameVariableTypeOption(type, getTypeName(type)))
            .ToArray();

    private readonly GameVariableService gameVariables;
    private readonly TextBox variableSearchBox;
    private readonly ListBox variableList;
    private readonly StackPanel detailPanel;
    private readonly TextBlock noSelectionText;
    private readonly ComboBox typeBox;
    private readonly ContentControl initialValueHost;
    private readonly TextBox remarkBox;
    private readonly Border errorBar;
    private readonly TextBlock errorText;
    private readonly Button deleteButton;
    private bool loading;
    private bool applyingServiceChange;

    public GameVariableManagerWindow(GameVariableService gameVariables)
    {
        this.gameVariables = gameVariables;
        Title = LocaleService.Get("GAME_VARIABLES");
        Width = 900;
        Height = 620;
        MinWidth = 700;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#1e1e1e"));
        FontFamily = FontFamily.Parse(
            "avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        variableSearchBox = EditorInputs.CreateEditableTextBox();
        variableSearchBox.PlaceholderText = LocaleService.Get("SEARCH");
        variableSearchBox.Margin = new Thickness(4);
        variableSearchBox.TextChanged += (_, _) => rebuildVariableList(null);
        variableList = new ListBox
        {
            Background = new SolidColorBrush(Color.FromRgb(40, 40, 40)),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        variableList.SelectionChanged += (_, _) => showSelectedVariable();

        Button addButton = new()
        {
            Content = "+ " + LocaleService.Get("NEW_GAME_VARIABLE"),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        addButton.Click += async (_, _) => await createVariableAsync();
        deleteButton = new Button
        {
            Content = LocaleService.Get("DELETE_GAME_VARIABLE"),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            IsEnabled = false,
        };
        deleteButton.Click += async (_, _) => await deleteSelectedAsync();
        Grid actions = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,8,*"),
            Margin = new Thickness(4),
        };
        actions.Children.Add(addButton);
        Grid.SetColumn(deleteButton, 2);
        actions.Children.Add(deleteButton);
        Grid left = new()
        {
            RowDefinitions = new RowDefinitions("Auto,*,Auto"),
        };
        left.Children.Add(variableSearchBox);
        Grid.SetRow(variableList, 1);
        left.Children.Add(variableList);
        Grid.SetRow(actions, 2);
        left.Children.Add(actions);

        typeBox = new ComboBox
        {
            ItemsSource = TypeOptions,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            MinHeight = EditorInputs.FieldMinHeight,
        };
        typeBox.SelectionChanged += (_, _) => changeSelectedType();
        initialValueHost = new ContentControl
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        remarkBox = EditorInputs.CreateEditableTextBox();
        remarkBox.AcceptsReturn = true;
        remarkBox.TextWrapping = TextWrapping.Wrap;
        remarkBox.Height = 120;
        remarkBox.MinHeight = 120;
        remarkBox.VerticalContentAlignment = VerticalAlignment.Top;
        remarkBox.TextChanged += (_, _) => changeSelectedRemark();

        Grid detailForm = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,12,*"),
            RowDefinitions = new RowDefinitions("Auto,12,Auto,12,Auto"),
        };
        addDetailRow(detailForm, 0, LocaleService.Get("GAME_VARIABLE_TYPE"), typeBox);
        addDetailRow(
            detailForm,
            2,
            LocaleService.Get("GAME_VARIABLE_INITIAL_VALUE"),
            initialValueHost);
        addDetailRow(detailForm, 4, LocaleService.Get("GAME_VARIABLE_REMARK"), remarkBox);
        detailPanel = new StackPanel
        {
            Margin = new Thickness(20),
            Spacing = 16,
            IsVisible = false,
            Children =
            {
                detailForm,
            },
        };
        noSelectionText = new TextBlock
        {
            Text = LocaleService.Get("NO_SELECTION"),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = Brushes.Gray,
        };
        Grid detailHost = new();
        detailHost.Children.Add(new ScrollViewer
        {
            Content = detailPanel,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
        });
        detailHost.Children.Add(noSelectionText);

        GridSplitter splitter = new()
        {
            Width = 4,
            Background = new SolidColorBrush(Color.FromRgb(50, 50, 50)),
            ResizeDirection = GridResizeDirection.Columns,
            VerticalAlignment = VerticalAlignment.Stretch,
        };
        Grid manager = new()
        {
            ColumnDefinitions = new ColumnDefinitions("240,4,*"),
        };
        manager.Children.Add(left);
        Grid.SetColumn(splitter, 1);
        manager.Children.Add(splitter);
        Grid.SetColumn(detailHost, 2);
        manager.Children.Add(detailHost);

        errorText = new TextBlock
        {
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
            Foreground = new SolidColorBrush(Color.Parse("#ffdddd")),
        };
        Button retryButton = new()
        {
            Content = LocaleService.Get("RETRY_SAVE"),
            MinWidth = 88,
        };
        retryButton.Click += (_, _) => retrySave();
        Grid errorContent = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,12,Auto"),
        };
        errorContent.Children.Add(errorText);
        Grid.SetColumn(retryButton, 2);
        errorContent.Children.Add(retryButton);
        errorBar = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#6b2929")),
            Padding = new Thickness(12, 8),
            Child = errorContent,
            IsVisible = false,
        };
        Grid root = new()
        {
            RowDefinitions = new RowDefinitions("Auto,*"),
        };
        root.Children.Add(errorBar);
        Grid.SetRow(manager, 1);
        root.Children.Add(manager);
        Content = root;

        gameVariables.Changed += onVariablesChanged;
        gameVariables.Saved += onVariablesSaved;
        Closed += onClosed;
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);

        GameVariableSaveResult ensureResult = applyChange(gameVariables.EnsureGeneratedFiles);
        rebuildVariableList(null);
        showSaveResult(ensureResult);
    }

    private static void addDetailRow(Grid form, int row, string label, Control editor)
    {
        TextBlock text = new()
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Top,
            Margin = new Thickness(0, 8, 0, 0),
        };
        Grid.SetRow(text, row);
        form.Children.Add(text);
        Grid.SetRow(editor, row);
        Grid.SetColumn(editor, 2);
        form.Children.Add(editor);
    }

    private void rebuildVariableList(string? preferredName)
    {
        string? current = preferredName ?? variableList.SelectedItem as string;
        string query = variableSearchBox.Text?.Trim() ?? string.Empty;
        string[] names = gameVariables.Variables
            .Select(variable => variable.Name)
            .Where(name => name.Contains(query, StringComparison.OrdinalIgnoreCase))
            .OrderBy(name => name, StringComparer.Ordinal)
            .ToArray();
        loading = true;
        variableList.ItemsSource = names;
        variableList.SelectedItem = current is not null && names.Contains(current, StringComparer.Ordinal)
            ? current
            : names.FirstOrDefault();
        loading = false;
        showSelectedVariable();
    }

    private void showSelectedVariable()
    {
        if (loading)
            return;
        if (variableList.SelectedItem is not string name
            || !gameVariables.TryGet(name, out GameVariableDefinition? definition)
            || definition is null)
        {
            detailPanel.IsVisible = false;
            noSelectionText.IsVisible = true;
            deleteButton.IsEnabled = false;
            initialValueHost.Content = null;
            return;
        }

        loading = true;
        typeBox.SelectedItem = TypeOptions.First(option => option.Type == definition.Type);
        remarkBox.Text = definition.Remark;
        rebuildInitialValueEditor(definition);
        detailPanel.IsVisible = true;
        noSelectionText.IsVisible = false;
        deleteButton.IsEnabled = true;
        loading = false;
    }

    private void rebuildInitialValueEditor(GameVariableDefinition definition)
    {
        BlueprintVariableForm form = new()
        {
            ShowFieldNames = false,
            MinWidth = 220,
        };
        BlueprintVariableField field = new(
            "initialValue",
            getTypeName(definition.Type),
            definition.InitialValue)
        {
            TypeName = getTypeName(definition.Type),
            PreserveNullValue = definition.InitialValue is null,
        };
        form.SetFields([field]);
        form.ValueChanged += (_, args) => changeSelectedInitialValue(args.Value);
        initialValueHost.Content = form;
    }

    private async Task createVariableAsync()
    {
        GameVariableCreation? request = await AddGameVariableDialog.ShowAsync(
            this,
            gameVariables.Variables.Select(variable => variable.Name),
            TypeOptions.Select(option => option.Label));
        if (request is null)
            return;
        GameVariableTypeOption? option = TypeOptions.FirstOrDefault(candidate =>
            string.Equals(candidate.Label, request.TypeName, StringComparison.Ordinal));
        if (option is null)
            return;
        GameVariableSaveResult result = applyChange(() =>
            gameVariables.Create(request.Name, option.Type));
        rebuildVariableList(request.Name);
        showSaveResult(result);
    }

    private async Task deleteSelectedAsync()
    {
        if (variableList.SelectedItem is not string name)
            return;
        string message = LocaleService.Get("CONFIRM_DELETE_GAME_VARIABLE")
            .Replace("{name}", name, StringComparison.Ordinal);
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            LocaleService.Get("DELETE_GAME_VARIABLE"),
            message);
        if (!confirmed)
            return;
        GameVariableSaveResult result = applyChange(() => gameVariables.Delete(name));
        rebuildVariableList(null);
        showSaveResult(result);
    }

    private void changeSelectedType()
    {
        if (loading
            || variableList.SelectedItem is not string name
            || typeBox.SelectedItem is not GameVariableTypeOption option
            || !gameVariables.TryGet(name, out GameVariableDefinition? current)
            || current is null
            || current.Type == option.Type)
        {
            return;
        }
        GameVariableSaveResult result = applyChange(() => gameVariables.ChangeType(name, option.Type));
        if (gameVariables.TryGet(name, out GameVariableDefinition? changed) && changed is not null)
        {
            loading = true;
            rebuildInitialValueEditor(changed);
            loading = false;
        }
        showSaveResult(result);
    }

    private void changeSelectedInitialValue(JsonNode? value)
    {
        if (loading || variableList.SelectedItem is not string name)
            return;
        GameVariableSaveResult result = applyChange(() =>
            gameVariables.SetInitialValue(name, value));
        showSaveResult(result);
    }

    private void changeSelectedRemark()
    {
        if (loading || variableList.SelectedItem is not string name)
            return;
        GameVariableSaveResult result = applyChange(() =>
            gameVariables.SetRemark(name, remarkBox.Text));
        showSaveResult(result);
    }

    private GameVariableSaveResult applyChange(Func<GameVariableSaveResult> action)
    {
        applyingServiceChange = true;
        try
        {
            return action();
        }
        finally
        {
            applyingServiceChange = false;
        }
    }

    private void retrySave()
    {
        showSaveResult(applyChange(gameVariables.SavePending));
    }

    private void showSaveResult(GameVariableSaveResult result)
    {
        if (result.Success)
        {
            errorText.Text = string.Empty;
            errorBar.IsVisible = false;
            return;
        }
        errorText.Text = LocaleService.Get("GAME_VARIABLE_SAVE_FAILED") + ": " + result.Detail;
        errorBar.IsVisible = true;
    }

    private void onVariablesChanged(object? sender, EventArgs args)
    {
        if (!applyingServiceChange)
            rebuildVariableList(null);
    }

    private void onVariablesSaved(object? sender, EventArgs args)
    {
        if (!gameVariables.IsModified)
        {
            errorText.Text = string.Empty;
            errorBar.IsVisible = false;
        }
    }

    private void onClosed(object? sender, EventArgs args)
    {
        gameVariables.Changed -= onVariablesChanged;
        gameVariables.Saved -= onVariablesSaved;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (EditorShortcuts.HasPrimaryModifier(args.KeyModifiers) && args.Key == Key.S)
        {
            retrySave();
            args.Handled = true;
        }
    }

    private static string getTypeName(GameVariableType type)
    {
        return type switch
        {
            GameVariableType.Bool => "bool",
            GameVariableType.Int => "int",
            GameVariableType.Float => "float",
            GameVariableType.String => "string",
            GameVariableType.List => "list",
            GameVariableType.Dict => "dict",
            _ => throw new ArgumentOutOfRangeException(nameof(type)),
        };
    }
}

internal sealed record GameVariableTypeOption(GameVariableType Type, string Label)
{
    public override string ToString()
    {
        return Label;
    }
}
