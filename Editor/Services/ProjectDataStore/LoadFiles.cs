using System;
using System.Collections.Generic;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal FileFingerprintCache FingerprintCache { get; } = new(ProjectDiskSnapshot.PathComparer);
    private HashSet<string>? loadSnapshotPaths;

    internal TResult[] readLoadFiles<TInput, TResult>(IReadOnlyList<TInput> inputs,
        Func<TInput, string> getPath, Func<TInput, ProjectLoadFile, TResult> prepare)
    {
        (TResult Result, string? Fingerprint)[] prepared = EditorValidationBatch.RunFiles(inputs,
            static () => new byte[128 * 1024], (buffer, input) =>
            {
                string path = getPath(input);
                reportDataRead(path);
                bool capture = loadSnapshotPaths?.Contains(path) == true;
                ProjectLoadFile file = new(path, FingerprintCache, buffer, loadCancellationToken, capture);
                TResult result = prepare(input, file);
                return (result, capture ? file.Fingerprint : null);
            }, cancellationToken: loadCancellationToken);
        TResult[] results = new TResult[inputs.Count];
        for (int index = 0; index < inputs.Count; index++)
        {
            results[index] = prepared[index].Result;
            if (prepared[index].Fingerprint is string fingerprint)
                InitialDiskSnapshot.Fingerprints[getPath(inputs[index])] = fingerprint;
        }
        return results;
    }
}
