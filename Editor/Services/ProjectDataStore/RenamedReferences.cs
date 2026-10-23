namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal RenamedReferenceState RenamedReferences { get; private set; } = RenamedReferenceState.Empty;

    internal void SetRenamedReferences(RenamedReferenceState state)
    {
        if (ReferenceEquals(RenamedReferences, state))
            return;
        RenamedReferenceState previous = RenamedReferences;
        Documents.TrackState(this, () => RenamedReferences = previous);
        RenamedReferences = state;
    }

    internal void ClearRenamedReferences() => SetRenamedReferences(RenamedReferenceState.Empty);
}
