using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialBlueprintAI.History;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialBlueprintAI.UI;

public sealed class BlueprintAssistantWindow : Window
{
    private readonly IBlueprintAssistantProvider provider;
    private readonly BlueprintAssistantProviderContext providerContext;
    private readonly IBlueprintAssistantHost assistantHost;
    private readonly BlueprintAssistantConversationStore conversationStore;
    private readonly ListBox conversationList = new();
    private readonly ComboBox blueprintSelector = new();
    private readonly StackPanel messageList = new() { Spacing = 10 };
    private readonly ScrollViewer messageScroll;
    private readonly TextBox inputBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBlock statusText = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
        TextWrapping = TextWrapping.Wrap,
    };
    private readonly Button newButton = new();
    private readonly Button renameButton = new();
    private readonly Button deleteButton = new();
    private readonly Button settingsButton = new();
    private readonly Button sendButton = new();
    private readonly Button stopButton = new();
    private readonly List<BlueprintAssistantMessage> messages = [];
    private readonly Dictionary<string, ProposalState> proposals = new(StringComparer.Ordinal);
    private IReadOnlyList<BlueprintAssistantConversationSummary> conversations = [];
    private BlueprintAssistantConversationSummary? currentConversation;
    private CancellationTokenSource? turnCancellation;
    private bool refreshingConversations;
    private bool turnRunning;
    private string suggestedBlueprintKey;

    public BlueprintAssistantWindow(
        IBlueprintAssistantProvider provider,
        BlueprintAssistantProviderContext providerContext,
        IBlueprintAssistantHost assistantHost,
        string title)
    {
        this.provider = provider;
        this.providerContext = providerContext;
        this.assistantHost = assistantHost;
        suggestedBlueprintKey = assistantHost.SuggestedBlueprintKey ?? string.Empty;
        conversationStore = new BlueprintAssistantConversationStore(
            providerContext.PluginDataDirectory);

        Title = title;
        Width = 1120;
        Height = 760;
        MinWidth = 840;
        MinHeight = 560;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#161718"));

        configureControls();
        messageScroll = new ScrollViewer
        {
            Content = messageList,
            Margin = new Thickness(14),
            VerticalScrollBarVisibility =
                Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
        };
        Content = createLayout();
        Opened += async (_, _) => await initializeAsync();
        Closed += onClosed;
        KeyDown += onKeyDown;
    }

    private void configureControls()
    {
        conversationList.SelectionChanged += async (_, _) =>
            await onConversationSelectionChangedAsync();
        blueprintSelector.HorizontalAlignment = HorizontalAlignment.Stretch;
        foreach (string key in assistantHost.ListBlueprints()
                     .OrderBy(key => key, StringComparer.Ordinal))
        {
            blueprintSelector.Items.Add(key);
        }
        if (blueprintSelector.Items.Count != 0)
        {
            blueprintSelector.SelectedItem =
                blueprintSelector.Items.Cast<string>().FirstOrDefault(key =>
                    string.Equals(key, suggestedBlueprintKey, StringComparison.Ordinal));
        }

        inputBox.AcceptsReturn = true;
        inputBox.TextWrapping = TextWrapping.Wrap;
        inputBox.Height = double.NaN;
        inputBox.MinHeight = 78;
        inputBox.MaxHeight = 180;
        inputBox.PlaceholderText = PluginUiText.Get("BLUEPRINT_AI_INPUT_HINT");
        inputBox.AddHandler(
            KeyDownEvent,
            onInputKeyDown,
            RoutingStrategies.Tunnel);

        newButton.Content = PluginUiText.Get("BLUEPRINT_AI_NEW_CONVERSATION");
        renameButton.Content = PluginUiText.Get("BLUEPRINT_AI_RENAME_CONVERSATION");
        deleteButton.Content = PluginUiText.Get("DELETE");
        settingsButton.Content = PluginUiText.Get("BLUEPRINT_AI_SETTINGS");
        sendButton.Content = PluginUiText.Get("SEND");
        stopButton.Content = PluginUiText.Get("BLUEPRINT_AI_STOP");
        stopButton.IsEnabled = false;
        newButton.Click += async (_, _) => await createConversationAsync(true);
        renameButton.Click += async (_, _) => await renameConversationAsync();
        deleteButton.Click += async (_, _) => await deleteConversationAsync();
        settingsButton.Click += (_, _) => showSettings();
        sendButton.Click += async (_, _) => await sendAsync();
        stopButton.Click += (_, _) => turnCancellation?.Cancel();
    }

    private Control createLayout()
    {
        StackPanel historyButtons = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 6,
        };
        historyButtons.Children.Add(newButton);
        historyButtons.Children.Add(renameButton);
        historyButtons.Children.Add(deleteButton);
        Grid history = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,*"),
            RowSpacing = 10,
        };
        history.Children.Add(historyButtons);
        Grid.SetRow(conversationList, 1);
        history.Children.Add(conversationList);

        TextBlock targetLabel = new()
        {
            Text = PluginUiText.Get("BLUEPRINT_AI_TARGET"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid targetBar = new()
        {
            Margin = new Thickness(14, 10),
            ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"),
            ColumnSpacing = 10,
        };
        targetBar.Children.Add(targetLabel);
        Grid.SetColumn(blueprintSelector, 1);
        targetBar.Children.Add(blueprintSelector);
        Grid.SetColumn(settingsButton, 2);
        targetBar.Children.Add(settingsButton);

        StackPanel actionButtons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        actionButtons.Children.Add(stopButton);
        actionButtons.Children.Add(sendButton);
        Grid inputPanel = new()
        {
            Margin = new Thickness(14, 0, 14, 14),
            RowDefinitions = new RowDefinitions("Auto,Auto"),
            RowSpacing = 8,
        };
        inputPanel.Children.Add(inputBox);
        Grid.SetRow(actionButtons, 1);
        inputPanel.Children.Add(actionButtons);

        Border statusBorder = new()
        {
            Padding = new Thickness(14, 8),
            Background = new SolidColorBrush(Color.Parse("#202124")),
            Child = statusText,
        };
        Grid assistant = new()
        {
            RowDefinitions = new RowDefinitions("Auto,*,Auto,Auto"),
        };
        assistant.Children.Add(targetBar);
        Grid.SetRow(messageScroll, 1);
        assistant.Children.Add(messageScroll);
        Grid.SetRow(statusBorder, 2);
        assistant.Children.Add(statusBorder);
        Grid.SetRow(inputPanel, 3);
        assistant.Children.Add(inputPanel);

        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("280,5,*"),
        };
        root.Children.Add(history);
        GridSplitter splitter = new()
        {
            Width = 5,
            ResizeDirection = GridResizeDirection.Columns,
            Background = new SolidColorBrush(Color.Parse("#353535")),
        };
        Grid.SetColumn(splitter, 1);
        root.Children.Add(splitter);
        Grid.SetColumn(assistant, 2);
        root.Children.Add(assistant);
        return root;
    }

    private async Task initializeAsync()
    {
        await refreshConversationIndexAsync(null);
    }

    private async Task refreshConversationIndexAsync(string? selectId)
    {
        IReadOnlyList<BlueprintAssistantConversationSummary> all =
            await conversationStore.LoadProjectIndexAsync(
                assistantHost.ProjectPath,
                CancellationToken.None);
        conversations = all;
        refreshingConversations = true;
        conversationList.Items.Clear();
        ConversationListItem? selected = null;
        foreach (BlueprintAssistantConversationSummary conversation in conversations)
        {
            ConversationListItem item = new(conversation);
            conversationList.Items.Add(item);
            if (string.Equals(conversation.Id, selectId, StringComparison.Ordinal))
                selected = item;
        }
        conversationList.SelectedItem = selected
            ?? conversationList.Items.OfType<ConversationListItem>().FirstOrDefault();
        refreshingConversations = false;
        if (conversationList.SelectedItem is null)
        {
            currentConversation = null;
            messages.Clear();
            proposals.Clear();
            messageList.Children.Clear();
            blueprintSelector.IsEnabled = true;
            sendButton.IsEnabled = false;
            inputBox.IsEnabled = false;
            renameButton.IsEnabled = false;
            deleteButton.IsEnabled = false;
        }
        await onConversationSelectionChangedAsync();
    }

    private async Task createConversationAsync(bool chooseTarget)
    {
        string? key = chooseTarget
            ? blueprintSelector.SelectedItem as string
            : suggestedBlueprintKey;
        if (chooseTarget)
        {
            key = await ItemSelectorDialog.ShowAsync(
                this,
                PluginUiText.Get("BLUEPRINT_AI_SELECT_TARGET"),
                PluginUiText.Get("BLUEPRINT_AI_TARGET_LOCKED_HINT"),
                assistantHost.ListBlueprints()
                    .OrderBy(value => value, StringComparer.Ordinal),
                suggestedBlueprintKey.Length == 0 ? key : suggestedBlueprintKey);
        }
        if (string.IsNullOrWhiteSpace(key))
        {
            statusText.Text = assistantHost.ListBlueprints().Count == 0
                ? PluginUiText.Get("BLUEPRINT_AI_NO_BLUEPRINTS")
                : string.Empty;
            return;
        }
        BlueprintAssistantConversationSummary conversation =
            await conversationStore.CreateAsync(
                assistantHost.ProjectPath,
                key,
                CancellationToken.None);
        suggestedBlueprintKey = key;
        await refreshConversationIndexAsync(conversation.Id);
        inputBox.Focus();
    }

    private async Task renameConversationAsync()
    {
        if (currentConversation is null)
            return;
        string? title = await SingleRowDialog.ShowAsync(
            this,
            PluginUiText.Get("BLUEPRINT_AI_RENAME_CONVERSATION"),
            PluginUiText.Get("BLUEPRINT_AI_CONVERSATION_NAME"),
            conversations
                .Where(conversation => conversation.Id != currentConversation.Id)
                .Select(conversation => conversation.Title),
            currentConversation.Title);
        if (title is null)
            return;
        await conversationStore.RenameAsync(
            currentConversation.Id,
            title,
            CancellationToken.None);
        await refreshConversationIndexAsync(currentConversation.Id);
    }

    private async Task deleteConversationAsync()
    {
        if (currentConversation is null)
            return;
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            PluginUiText.Get("DELETE_CONFIRMATION"),
            PluginUiText.Get("BLUEPRINT_AI_DELETE_CONVERSATION_PROMPT"));
        if (!confirmed)
            return;
        string deletedId = currentConversation.Id;
        currentConversation = null;
        await conversationStore.DeleteAsync(deletedId, CancellationToken.None);
        await refreshConversationIndexAsync(null);
    }

    private async Task onConversationSelectionChangedAsync()
    {
        if (refreshingConversations
            || conversationList.SelectedItem is not ConversationListItem item)
        {
            return;
        }
        currentConversation = item.Conversation;
        blueprintSelector.SelectedItem = currentConversation.BlueprintKey;
        blueprintSelector.IsEnabled = false;
        renameButton.IsEnabled = true;
        deleteButton.IsEnabled = true;
        await loadConversationAsync(currentConversation);
    }

    private async Task loadConversationAsync(
        BlueprintAssistantConversationSummary conversation)
    {
        messages.Clear();
        proposals.Clear();
        messageList.Children.Clear();
        IReadOnlyList<BlueprintAssistantConversationEntry> entries =
            await conversationStore.LoadEntriesAsync(
                conversation.Id,
                CancellationToken.None);
        Dictionary<string, BlueprintAssistantConversationEntry> storedProposals =
            new(StringComparer.Ordinal);
        Dictionary<string, string> completedProposals = new(StringComparer.Ordinal);
        bool targetExists = assistantHost.ListBlueprints().Contains(
            conversation.BlueprintKey,
            StringComparer.Ordinal);
        bool incompleteTurn = false;
        foreach (BlueprintAssistantConversationEntry entry in entries)
        {
            if (entry.Type == "message" && entry.Role is BlueprintAssistantMessageRole role)
            {
                BlueprintAssistantMessage message = new(
                    role,
                    entry.Content,
                    entry.CreatedAt);
                messages.Add(message);
                addMessage(message);
                if (role == BlueprintAssistantMessageRole.User)
                    incompleteTurn = true;
            }
            else if (entry.Type == "turnStarted")
            {
                incompleteTurn = true;
            }
            else if (entry.Type == "proposal")
            {
                storedProposals[entry.ProposalId] = entry;
            }
            else if (entry.Type == "proposalAction")
            {
                completedProposals[entry.ProposalId] = entry.Action;
            }
            else if (entry.Type == "interrupted")
            {
                addSystemMessage(PluginUiText.Get("BLUEPRINT_AI_INTERRUPTED"));
                incompleteTurn = false;
            }
            else if (entry.Type == "failed")
            {
                addSystemMessage(
                    PluginUiText.Get("BLUEPRINT_AI_FAILED")
                        .Replace("{error}", entry.Content, StringComparison.Ordinal));
                incompleteTurn = false;
            }
            else if (entry.Type == "turnCompleted")
            {
                incompleteTurn = false;
            }
        }
        if (incompleteTurn)
        {
            addSystemMessage(PluginUiText.Get("BLUEPRINT_AI_INTERRUPTED"));
            await conversationStore.AppendInterruptedAsync(
                conversation.Id,
                CancellationToken.None);
        }
        foreach (BlueprintAssistantConversationEntry entry in storedProposals.Values)
        {
            BlueprintAssistantProposal proposal = new(
                entry.ProposalId,
                entry.ProposalTitle,
                entry.ProposalDiff,
                entry.ProposalBaseRevision,
                entry.ProposalValid,
                entry.Diagnostics);
            completedProposals.TryGetValue(entry.ProposalId, out string? action);
            ProposalState state = new(
                proposal,
                entry.ProposalCandidate,
                action ?? string.Empty,
                null);
            proposals[proposal.Id] = state;
            addProposal(state, targetExists);
        }
        sendButton.IsEnabled = targetExists && !turnRunning;
        inputBox.IsEnabled = targetExists && !turnRunning;
        statusText.Text = targetExists
            ? string.Empty
            : PluginUiText.Get("BLUEPRINT_AI_TARGET_MISSING");
        scrollToEnd();
    }

    private async Task sendAsync()
    {
        if (turnRunning
            || currentConversation is null
            || string.IsNullOrWhiteSpace(inputBox.Text))
        {
            return;
        }
        string prompt = inputBox.Text.Trim();
        string conversationId = currentConversation.Id;
        string blueprintKey = currentConversation.BlueprintKey;
        CancellationTokenSource cancellationSource = new();
        turnCancellation = cancellationSource;
        CancellationToken cancellationToken = cancellationSource.Token;
        LiveAssistantMessage? assistantMessageView = null;
        StringBuilder assistantContent = new();
        bool assistantMessageFinalized = false;
        setTurnRunning(true);
        try
        {
            if (!assistantHost.ListBlueprints().Contains(
                blueprintKey,
                StringComparer.Ordinal))
            {
                statusText.Text = PluginUiText.Get("BLUEPRINT_AI_TARGET_MISSING");
                return;
            }
            IBlueprintAssistantSession session =
                assistantHost.CreateSession(blueprintKey);
            BlueprintAssistantSettings turnSettings =
                await provider.LoadSettingsAsync(
                    providerContext,
                    cancellationToken);
            await conversationStore.AppendTurnStartedAsync(
                conversationId,
                turnSettings.Provider,
                turnSettings.Model,
                turnSettings.Endpoint,
                session.BaseRevision,
                cancellationToken);
            BlueprintAssistantMessage userMessage = new(
                BlueprintAssistantMessageRole.User,
                prompt,
                DateTimeOffset.UtcNow);
            messages.Add(userMessage);
            addMessage(userMessage);
            await conversationStore.AppendMessageAsync(
                conversationId,
                userMessage,
                cancellationToken);
            inputBox.Text = string.Empty;
            if (currentConversation.Title == currentConversation.BlueprintKey)
            {
                string generatedTitle = prompt.Length <= 42 ? prompt : prompt[..42];
                await conversationStore.RenameAsync(
                    conversationId,
                    generatedTitle,
                    cancellationToken);
            }

            assistantMessageView = createLiveAssistantMessage();
            BlueprintAssistantTurnContext context = new(
                providerContext,
                conversationId,
                assistantHost.ProjectPath,
                blueprintKey,
                session.BaseRevision,
                messages.ToArray(),
                session.Workspace,
                cancellationToken);
            await foreach (BlueprintAssistantEvent assistantEvent in
                           provider.RunTurnAsync(context))
            {
                cancellationToken.ThrowIfCancellationRequested();
                await handleAssistantEventAsync(
                    assistantEvent,
                    session,
                    assistantMessageView.Text,
                    assistantContent,
                    conversationId,
                    cancellationToken);
            }
            assistantMessageFinalized = true;
            await finalizeAssistantMessageAsync(
                assistantMessageView,
                assistantContent,
                conversationId);
            await conversationStore.AppendTurnCompletedAsync(
                conversationId,
                CancellationToken.None);
            statusText.Text = string.Empty;
        }
        catch (OperationCanceledException)
        {
            if (!assistantMessageFinalized)
            {
                assistantMessageFinalized = true;
                await finalizeAssistantMessageAsync(
                    assistantMessageView,
                    assistantContent,
                    conversationId);
            }
            statusText.Text = PluginUiText.Get("BLUEPRINT_AI_INTERRUPTED");
            await conversationStore.AppendInterruptedAsync(
                conversationId,
                CancellationToken.None);
        }
        catch (Exception exception)
        {
            if (!assistantMessageFinalized)
            {
                assistantMessageFinalized = true;
                await finalizeAssistantMessageAsync(
                    assistantMessageView,
                    assistantContent,
                    conversationId);
            }
            string error = BlueprintAssistantText.SanitizeError(exception.Message);
            statusText.Text = PluginUiText.Get("BLUEPRINT_AI_FAILED")
                .Replace("{error}", error, StringComparison.Ordinal);
            await conversationStore.AppendFailedAsync(
                conversationId,
                error,
                CancellationToken.None);
        }
        finally
        {
            cancellationSource.Dispose();
            if (ReferenceEquals(turnCancellation, cancellationSource))
                turnCancellation = null;
            if (IsVisible)
            {
                setTurnRunning(false);
                await refreshConversationIndexAsync(conversationId);
            }
        }
    }

    private async Task finalizeAssistantMessageAsync(
        LiveAssistantMessage? assistantMessageView,
        StringBuilder assistantContent,
        string conversationId)
    {
        if (assistantMessageView is null)
        {
            return;
        }
        if (assistantContent.Length == 0)
        {
            messageList.Children.Remove(assistantMessageView.Border);
            return;
        }

        string content = assistantContent.ToString();
        assistantMessageView.Host.Content =
            BlueprintAssistantMarkdownRenderer.Create(content);
        BlueprintAssistantMessage assistantMessage = new(
            BlueprintAssistantMessageRole.Assistant,
            content,
            DateTimeOffset.UtcNow);
        messages.Add(assistantMessage);
        await conversationStore.AppendMessageAsync(
            conversationId,
            assistantMessage,
            CancellationToken.None);
    }

    private async Task handleAssistantEventAsync(
        BlueprintAssistantEvent assistantEvent,
        IBlueprintAssistantSession session,
        SelectableTextBlock assistantText,
        StringBuilder assistantContent,
        string conversationId,
        CancellationToken cancellationToken)
    {
        switch (assistantEvent.Kind)
        {
            case BlueprintAssistantEventKind.TextDelta:
                assistantContent.Append(assistantEvent.Text);
                assistantText.Text = assistantContent.ToString();
                scrollToEnd();
                break;
            case BlueprintAssistantEventKind.Status:
                statusText.Text = safeStatus(assistantEvent.Text, 500);
                break;
            case BlueprintAssistantEventKind.ToolStarted:
                string startedTool = safeStatus(assistantEvent.ToolName, 100);
                statusText.Text = PluginUiText.Get("BLUEPRINT_AI_TOOL_RUNNING")
                    .Replace("{tool}", startedTool, StringComparison.Ordinal);
                await conversationStore.AppendToolAuditAsync(
                    conversationId,
                    startedTool,
                    string.Empty,
                    true,
                    cancellationToken);
                break;
            case BlueprintAssistantEventKind.ToolCompleted:
                string completedTool = safeStatus(assistantEvent.ToolName, 100);
                string completedStatus = safeStatus(assistantEvent.Text, 240);
                string auditStatus = completedStatus.Contains(
                        "fail",
                        StringComparison.OrdinalIgnoreCase)
                    || completedStatus.Contains(
                        "error",
                        StringComparison.OrdinalIgnoreCase)
                        ? "failed"
                        : "completed";
                statusText.Text = completedStatus.Length == 0
                    ? completedTool
                    : completedTool + ": " + completedStatus;
                await conversationStore.AppendToolAuditAsync(
                    conversationId,
                    completedTool,
                    auditStatus,
                    false,
                    cancellationToken);
                break;
            case BlueprintAssistantEventKind.ProposalReady:
                if (assistantEvent.Proposal is not BlueprintAssistantProposal proposal)
                    break;
                BlueprintAssistantCandidateResult candidateResult =
                    await session.GetProposalCandidateAsync(
                        proposal.Id,
                        cancellationToken);
                if (!candidateResult.Success)
                {
                    statusText.Text = BlueprintAssistantText.SanitizeError(
                        candidateResult.Error);
                    break;
                }
                ProposalState state = new(
                    proposal,
                    candidateResult.CandidateJson,
                    string.Empty,
                    session);
                proposals[proposal.Id] = state;
                await conversationStore.AppendProposalAsync(
                    conversationId,
                    proposal,
                    candidateResult.CandidateJson,
                    cancellationToken);
                addProposal(state);
                break;
            case BlueprintAssistantEventKind.Completed:
                statusText.Text = string.Empty;
                break;
        }
    }

    private LiveAssistantMessage createLiveAssistantMessage()
    {
        SelectableTextBlock content = new()
        {
            TextWrapping = TextWrapping.Wrap,
        };
        ContentControl host = new() { Content = content };
        Border border = createMessageBorder(
            PluginUiText.Get("BLUEPRINT_AI_ASSISTANT"),
            host,
            false);
        messageList.Children.Add(border);
        scrollToEnd();
        return new LiveAssistantMessage(border, host, content);
    }

    private void addMessage(BlueprintAssistantMessage message)
    {
        Control content = message.Role == BlueprintAssistantMessageRole.Assistant
            ? BlueprintAssistantMarkdownRenderer.Create(message.Content)
            : new SelectableTextBlock
            {
                Text = message.Content,
                TextWrapping = TextWrapping.Wrap,
            };
        string role = message.Role == BlueprintAssistantMessageRole.User
            ? PluginUiText.Get("BLUEPRINT_AI_YOU")
            : PluginUiText.Get("BLUEPRINT_AI_ASSISTANT");
        messageList.Children.Add(createMessageBorder(
            role,
            content,
            message.Role == BlueprintAssistantMessageRole.User));
    }

    private void addSystemMessage(string text)
    {
        TextBlock message = new()
        {
            Text = text,
            Foreground = new SolidColorBrush(Color.Parse("#9e9e9e")),
            TextWrapping = TextWrapping.Wrap,
            HorizontalAlignment = HorizontalAlignment.Center,
        };
        messageList.Children.Add(message);
    }

    private Border createMessageBorder(
        string role,
        Control content,
        bool user)
    {
        TextBlock roleText = new()
        {
            Text = role,
            FontWeight = FontWeight.SemiBold,
            Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
        };
        StackPanel panel = new() { Spacing = 6 };
        panel.Children.Add(roleText);
        panel.Children.Add(content);
        return new Border
        {
            Background = new SolidColorBrush(Color.Parse(user ? "#263248" : "#242526")),
            CornerRadius = new CornerRadius(6),
            Padding = new Thickness(12),
            Child = panel,
        };
    }

    private void addProposal(ProposalState state, bool targetExists = true)
    {
        TextBlock title = new()
        {
            Text = state.Proposal.Title,
            FontWeight = FontWeight.SemiBold,
            FontSize = 16,
        };
        TextBox diff = new()
        {
            Text = state.Proposal.Diff,
            IsReadOnly = true,
            AcceptsReturn = true,
            TextWrapping = TextWrapping.NoWrap,
            FontFamily = FontFamily.Parse("Consolas"),
            MinHeight = 120,
            MaxHeight = 280,
        };
        TextBlock diagnostics = new()
        {
            Text = state.Proposal.IsValid
                ? PluginUiText.Get("BLUEPRINT_AI_PROPOSAL_VALID")
                : string.Join(Environment.NewLine, state.Proposal.Diagnostics),
            Foreground = new SolidColorBrush(Color.Parse(
                state.Proposal.IsValid ? "#81c784" : "#ef9a9a")),
            TextWrapping = TextWrapping.Wrap,
        };
        Button apply = new()
        {
            Content = PluginUiText.Get("BLUEPRINT_AI_APPLY"),
            IsEnabled = targetExists
                && state.Proposal.IsValid
                && !state.Completed,
        };
        Button discard = new()
        {
            Content = PluginUiText.Get("BLUEPRINT_AI_DISCARD"),
            IsEnabled = !state.Completed,
        };
        TextBlock result = new()
        {
            Text = state.Completed
                ? state.Action == "applied"
                    ? PluginUiText.Get("BLUEPRINT_AI_PROPOSAL_APPLIED")
                    : PluginUiText.Get("BLUEPRINT_AI_PROPOSAL_DISCARDED")
                : string.Empty,
            Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
            VerticalAlignment = VerticalAlignment.Center,
        };
        apply.Click += async (_, _) =>
            await applyProposalAsync(state, apply, discard, result);
        discard.Click += async (_, _) =>
            await discardProposalAsync(state, apply, discard, result);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(result);
        buttons.Children.Add(discard);
        buttons.Children.Add(apply);
        StackPanel panel = new() { Spacing = 8 };
        panel.Children.Add(title);
        panel.Children.Add(diff);
        panel.Children.Add(diagnostics);
        panel.Children.Add(buttons);
        messageList.Children.Add(new Border
        {
            Background = new SolidColorBrush(Color.Parse("#2b2a24")),
            BorderBrush = new SolidColorBrush(Color.Parse("#575141")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(6),
            Padding = new Thickness(12),
            Child = panel,
        });
        scrollToEnd();
    }

    private async Task applyProposalAsync(
        ProposalState state,
        Button applyButton,
        Button discardButton,
        TextBlock resultText)
    {
        if (currentConversation is null || state.Completed)
            return;
        bool targetExists = assistantHost.ListBlueprints().Contains(
            currentConversation.BlueprintKey,
            StringComparer.Ordinal);
        if (!targetExists)
        {
            applyButton.IsEnabled = false;
            resultText.Text = PluginUiText.Get("BLUEPRINT_AI_TARGET_MISSING");
            return;
        }
        applyButton.IsEnabled = false;
        discardButton.IsEnabled = false;
        try
        {
            IBlueprintAssistantSession session = state.Session ??
                assistantHost.CreateSession(currentConversation.BlueprintKey);
            BlueprintAssistantApplyResult result = state.Session is not null
                ? await session.ApplyProposalAsync(
                    state.Proposal.Id,
                    CancellationToken.None)
                : await session.ApplyCandidateAsync(
                    state.Proposal.BaseRevision,
                    state.CandidateJson,
                    CancellationToken.None);
            if (!result.Success)
            {
                resultText.Text = result.Conflict
                    ? PluginUiText.Get("BLUEPRINT_AI_PROPOSAL_CONFLICT")
                    : BlueprintAssistantText.SanitizeError(result.Error);
                applyButton.IsEnabled = state.Proposal.IsValid;
                discardButton.IsEnabled = true;
                return;
            }
            state.Action = "applied";
            resultText.Text = PluginUiText.Get("BLUEPRINT_AI_PROPOSAL_APPLIED");
            await conversationStore.AppendProposalActionAsync(
                currentConversation.Id,
                state.Proposal.Id,
                "applied",
                CancellationToken.None);
        }
        catch (Exception exception)
        {
            resultText.Text = BlueprintAssistantText.SanitizeError(
                exception.Message);
            applyButton.IsEnabled =
                assistantHost.ListBlueprints().Contains(
                    currentConversation.BlueprintKey,
                    StringComparer.Ordinal)
                && state.Proposal.IsValid;
            discardButton.IsEnabled = true;
        }
    }

    private async Task discardProposalAsync(
        ProposalState state,
        Button applyButton,
        Button discardButton,
        TextBlock resultText)
    {
        if (currentConversation is null || state.Completed)
            return;
        if (state.Session is not null)
        {
            PluginResult discardResult =
                await state.Session.DiscardProposalAsync(
                    state.Proposal.Id,
                    CancellationToken.None);
            if (!discardResult.Success)
            {
                resultText.Text = BlueprintAssistantText.SanitizeError(
                    discardResult.Error);
                return;
            }
        }
        state.Action = "discarded";
        applyButton.IsEnabled = false;
        discardButton.IsEnabled = false;
        resultText.Text = PluginUiText.Get("BLUEPRINT_AI_PROPOSAL_DISCARDED");
        await conversationStore.AppendProposalActionAsync(
            currentConversation.Id,
            state.Proposal.Id,
            "discarded",
            CancellationToken.None);
    }

    private void showSettings()
    {
        BlueprintAssistantSettingsWindow window = new(
            provider,
            providerContext);
        window.Show(this);
    }

    private void setTurnRunning(bool running)
    {
        turnRunning = running;
        sendButton.IsEnabled = !running
            && currentConversation is not null
            && assistantHost.ListBlueprints().Contains(
                currentConversation.BlueprintKey,
                StringComparer.Ordinal);
        stopButton.IsEnabled = running;
        inputBox.IsEnabled = !running && sendButton.IsEnabled;
        newButton.IsEnabled = !running;
        renameButton.IsEnabled = !running && currentConversation is not null;
        deleteButton.IsEnabled = !running && currentConversation is not null;
        conversationList.IsEnabled = !running;
        settingsButton.IsEnabled = !running;
    }

    private void scrollToEnd()
    {
        Dispatcher.UIThread.Post(() =>
        {
            messageScroll.Offset = new Vector(
                messageScroll.Offset.X,
                Math.Max(0, messageScroll.Extent.Height));
        });
    }

    private void onInputKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Enter
            || args.KeyModifiers.HasFlag(KeyModifiers.Shift))
        {
            return;
        }
        args.Handled = true;
        _ = sendAsync();
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape && turnRunning)
        {
            turnCancellation?.Cancel();
            args.Handled = true;
        }
    }

    private void onClosed(object? sender, EventArgs args)
    {
        turnCancellation?.Cancel();
    }

    private static string safeStatus(string value, int maximumLength)
    {
        string sanitized = BlueprintAssistantText.SanitizeError(value)
            .Replace('\r', ' ')
            .Replace('\n', ' ')
            .Trim();
        return sanitized.Length <= maximumLength
            ? sanitized
            : sanitized[..maximumLength];
    }

    private sealed class ConversationListItem
    {
        public ConversationListItem(BlueprintAssistantConversationSummary conversation)
        {
            Conversation = conversation;
        }

        public BlueprintAssistantConversationSummary Conversation { get; }

        public override string ToString()
        {
            return Conversation.Title + Environment.NewLine + Conversation.BlueprintKey;
        }
    }

    private sealed class ProposalState
    {
        public ProposalState(
            BlueprintAssistantProposal proposal,
            string candidateJson,
            string action,
            IBlueprintAssistantSession? session)
        {
            Proposal = proposal;
            CandidateJson = candidateJson;
            Action = action;
            Session = session;
        }

        public BlueprintAssistantProposal Proposal { get; }
        public string CandidateJson { get; }
        public string Action { get; set; }
        public IBlueprintAssistantSession? Session { get; }
        public bool Completed => Action.Length != 0;
    }

    private sealed record LiveAssistantMessage(
        Border Border,
        ContentControl Host,
        SelectableTextBlock Text);
}
