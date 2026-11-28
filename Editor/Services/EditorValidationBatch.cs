using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class EditorValidationBatch
{
    internal static int FileWorkers => Environment.ProcessorCount;

    internal static TResult[] Run<TInput, TResult>(IReadOnlyList<TInput> inputs,
        Func<TInput, TResult> action, CancellationToken cancellationToken = default,
        Action<int>? completed = null)
        => Run(inputs, static () => 0, (_, input) => action(input),
            cancellationToken: cancellationToken, completed: completed);

    internal static TResult[] Run<TInput, TState, TResult>(IReadOnlyList<TInput> inputs,
        Func<TState> createState, Func<TState, TInput, TResult> action,
        Action<TState>? disposeState = null, CancellationToken cancellationToken = default,
        Action<int>? completed = null)
        => FileBatch.Run(inputs, createState, action, disposeState, cancellationToken, completed,
            maxWorkers: Math.Max(1, Environment.ProcessorCount - 1), allowNestedParallelism: false);

    internal static TResult[] RunFiles<TInput, TResult>(IReadOnlyList<TInput> inputs,
        Func<TInput, TResult> action, CancellationToken cancellationToken = default,
        Action<int>? completed = null)
        => FileBatch.Run(inputs, action, cancellationToken, completed);

    internal static TResult[] RunFiles<TInput, TState, TResult>(IReadOnlyList<TInput> inputs,
        Func<TState> createState, Func<TState, TInput, TResult> action,
        Action<TState>? disposeState = null, CancellationToken cancellationToken = default,
        Action<int>? completed = null)
        => FileBatch.Run(inputs, createState, action, disposeState, cancellationToken, completed);

    internal static void ForEachFile<TInput>(IReadOnlyList<TInput> inputs, Action<TInput> action,
        CancellationToken cancellationToken = default, Action<int>? completed = null)
        => RunFiles(inputs, input =>
        {
            action(input);
            return true;
        }, cancellationToken, completed);

    internal static Task ForEachFileAsync<TInput>(IReadOnlyList<TInput> inputs,
        Func<TInput, CancellationToken, ValueTask> action, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (inputs.Count == 0)
            return Task.CompletedTask;
        return Parallel.ForEachAsync(inputs, new ParallelOptions
        {
            MaxDegreeOfParallelism = Math.Min(inputs.Count, FileWorkers),
            CancellationToken = cancellationToken,
            TaskScheduler = TaskScheduler.Default,
        }, action);
    }
}
