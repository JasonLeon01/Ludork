using CommunityToolkit.Mvvm.Input;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

internal sealed class GeneralDataPageViewModel : ViewModelBase
{
    private readonly GeneralDataService generalData;
    private readonly EditorDocument? document;
    private readonly string initialTypeKey;
    private readonly GeneralDataPageSessionState session;
    private readonly Dictionary<JsonObject, string> memberKeys = [];
    private GeneralDataTypeSnapshot typeData;
    private IReadOnlyDictionary<string, JsonObject> members;
    private JsonObject parameters;
    private long documentRevision;
    private bool active;

    public GeneralDataPageViewModel(
        GeneralDataService generalData,
        EditorDocument? document,
        string typeKey,
        GeneralDataTypeSnapshot typeData,
        GeneralDataPageSessionState session)
    {
        this.generalData = generalData;
        this.document = document;
        initialTypeKey = typeKey;
        this.typeData = typeData;
        members = typeData.Members;
        parameters = typeData.ParameterDefinitions;
        this.session = session;
        documentRevision = document?.Revision ?? 0;
        rebuildMemberKeys();
        FormViewCommand = new RelayCommand(() => ViewMode = GeneralDataViewMode.Form);
        TableViewCommand = new RelayCommand(() => ViewMode = GeneralDataViewMode.Table);
    }

    public string TypeKey => document?.Key ?? initialTypeKey;
    public JsonObject ParameterDefinitions => parameters;
    public JsonObject? GetMember(string? memberId) => memberId is null ? null : members.GetValueOrDefault(memberId);
    public long DocumentRevision => documentRevision;
    public long CurrentDocumentRevision => document?.Revision ?? 0;
    public IRelayCommand FormViewCommand { get; }
    public IRelayCommand TableViewCommand { get; }
    public event EventHandler? DocumentChanged;

    public string SearchText
    {
        get => session.SearchText;
        set
        {
            if (session.SearchText == value)
                return;
            session.SearchText = value;
            OnPropertyChanged();
        }
    }

    public string? SelectedMemberId
    {
        get => session.SelectedMemberId;
        set
        {
            if (session.SelectedMemberId == value)
                return;
            session.SelectedMemberId = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(CanEditSelectedAbilityGraph));
        }
    }

    public GeneralDataViewMode ViewMode
    {
        get => session.ViewMode;
        set
        {
            if (session.ViewMode == value)
                return;
            session.ViewMode = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(IsFormView));
            OnPropertyChanged(nameof(IsTableView));
        }
    }

    public bool IsFormView => ViewMode == GeneralDataViewMode.Form;
    public bool IsTableView => !IsFormView;
    public bool CanEditSelectedAbilityGraph => CanEditAbilityGraph(SelectedMemberId);
    public bool HasEvents => typeData.Events.Count != 0;
    public IEnumerable<string> MemberIds => members.Keys;
    public IEnumerable<string> ParameterNames => parameters.Select(entry => entry.Key);
    public IEnumerable<string> ReferenceTypeKeys => generalData.GeneralData.Keys.OrderBy(key => key, StringComparer.Ordinal);

    public void Activate()
    {
        if (active)
            return;
        active = true;
        if (document is not null)
            document.Changed += onDocumentChanged;
    }

    public void Deactivate()
    {
        if (!active)
            return;
        active = false;
        if (document is not null)
            document.Changed -= onDocumentChanged;
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        DocumentChanged?.Invoke(this, args);
    }

    public bool RefreshFromDocument()
    {
        if (document is null || documentRevision == document.Revision || document.Data is not JsonObject current)
            return false;
        documentRevision = document.Revision;
        if (JsonNode.DeepEquals(current, typeData.ToJson()))
            return false;
        setTypeData(new GeneralDataTypeSnapshot(current));
        return true;
    }

    public IReadOnlyList<string> FilterMembers()
    {
        return members.Where(entry => MatchesSearch(entry.Key, entry.Value))
                .Select(entry => entry.Key).OrderBy(id => id, StringComparer.Ordinal).ToArray();
    }

    public string? ChooseSelection(IReadOnlyList<string> memberIds, string? preferredMemberId)
    {
        return preferredMemberId is not null && memberIds.Contains(preferredMemberId)
            ? preferredMemberId
            : SelectedMemberId is not null && memberIds.Contains(SelectedMemberId)
                ? SelectedMemberId
                : memberIds.FirstOrDefault();
    }

    public bool MatchesSearch(string memberId, JsonObject member)
    {
        string query = SearchText.Trim();
        return query.Length == 0 || memberId.Contains(query, StringComparison.OrdinalIgnoreCase)
            || member.Any(entry => entry.Value is JsonValue scalar
                && scalar.ToJsonString().Contains(query, StringComparison.OrdinalIgnoreCase));
    }

    public IReadOnlyList<GeneralDataTableColumn> GetColumns()
    {
        return parameters.Where(entry => entry.Value is JsonObject)
                .Select(entry => new GeneralDataTableColumn(entry.Key, (JsonObject)entry.Value!)).ToArray();
    }

    public GeneralDataTableRow[] GetRows()
    {
        return members.Where(entry => MatchesSearch(entry.Key, entry.Value))
                .OrderBy(entry => entry.Key, StringComparer.Ordinal)
                .Select(entry => new GeneralDataTableRow(entry.Key, entry.Value)).ToArray();
    }

    public bool CanEditAbilityGraph(string? memberId)
    {
        return !string.IsNullOrWhiteSpace(memberId) && members.ContainsKey(memberId) && HasEvents;
    }

    public bool ContainsMember(string memberId) => members.ContainsKey(memberId);

    public string SuggestDuplicateId(string memberId)
    {
        HashSet<string> ids = MemberIds.ToHashSet(StringComparer.Ordinal);
        string next = memberId + "_copy";
        int counter = 2;
        while (ids.Contains(next))
            next = memberId + "_copy" + counter++;
        return next;
    }

    public bool CreateMember(string memberId) => reloadAfter(generalData.CreateGeneralMember(TypeKey, memberId));
    public bool RenameMember(string oldId, string newId) => reloadAfter(generalData.RenameGeneralMember(TypeKey, oldId, newId));
    public bool DuplicateMember(string sourceId, string newId) => reloadAfter(generalData.DuplicateGeneralMember(TypeKey, sourceId, newId));
    public bool DeleteMember(string memberId) => reloadAfter(generalData.DeleteGeneralMember(TypeKey, memberId));

    public bool AddParameter(GeneralDataParamCreation value)
    {
        return reloadAfter(generalData.AddGeneralParameter(TypeKey, value.Name, GeneralDataParameterSchema.BuildParamDefinition(value)));
    }

    public GeneralDataParamCreation? GetParameter(string name)
    {
        return parameters[name] is JsonObject definition
            ? GeneralDataParameterSchema.CreateParamCreation(name, definition)
            : null;
    }

    public bool UpdateParameter(string name, GeneralDataParamCreation initialValue, GeneralDataParamCreation value)
    {
        if (parameters[name] is not JsonObject definition)
            return false;
        JsonObject next = GeneralDataParameterSchema.UpdateParamDefinition(definition, initialValue, value);
        bool reset = GeneralDataParameterSchema.HasValueTypeChanged(initialValue, value);
        if (value.Name == name && JsonNode.DeepEquals(definition, next))
            return false;
        return reloadAfter(generalData.UpdateGeneralParameter(TypeKey, name, value.Name, next, reset));
    }

    public bool DeleteParameter(string name) => reloadAfter(generalData.DeleteGeneralParameter(TypeKey, name));

    public bool UpdateParameterReference(JsonObject definition, JsonObject? reference)
    {
        if (JsonNode.DeepEquals(definition["reference"], reference))
            return false;
        string? name = parameters.FirstOrDefault(
            entry => ReferenceEquals(entry.Value, definition)).Key;
        return name is not null && reloadAfter(generalData.UpdateGeneralParameterReference(TypeKey, name, reference));
    }

    public bool UpdateMemberValue(JsonObject member, string name, JsonNode? value)
    {
        if (!memberKeys.TryGetValue(member, out string? memberId)
            || !generalData.UpdateGeneralMemberValue(TypeKey, memberId, name, value))
            return false;
        member[name] = value?.DeepClone();
        typeData.ApplyMemberValue(memberId, name, value);
        return true;
    }

    public List<string> GetReferenceMemberIds(string typeKey)
    {
        return generalData.GeneralData.TryGetValue(typeKey, out GeneralDataTypeSnapshot? data)
            ? data.Members.Keys.OrderBy(key => key, StringComparer.Ordinal).ToList()
            : [];
    }

    private bool reloadAfter(bool changed)
    {
        if (changed)
            setTypeData(generalData.GeneralData.TryGetValue(TypeKey, out GeneralDataTypeSnapshot? current) ? current : new GeneralDataTypeSnapshot([]));
        return changed;
    }

    private void setTypeData(GeneralDataTypeSnapshot current)
    {
        typeData = current;
        members = current.Members;
        parameters = current.ParameterDefinitions;
        documentRevision = document?.Revision ?? documentRevision;
        rebuildMemberKeys();
        OnPropertyChanged(nameof(HasEvents));
        OnPropertyChanged(nameof(CanEditSelectedAbilityGraph));
    }

    private void rebuildMemberKeys()
    {
        memberKeys.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in members)
            memberKeys[entry.Value] = entry.Key;
    }
}
