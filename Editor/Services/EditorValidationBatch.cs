using System;
using System.Collections.Generic;
using System.Runtime.ExceptionServices;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class EditorValidationBatch
{
    [ThreadStatic]
    private static int depth;

    internal static TResult[] Run<TInput, TResult>(IReadOnlyList<TInput> inputs,
        Func<TInput, TResult> action, CancellationToken cancellationToken = default,
        Action<int>? completed = null)
        => Run(inputs, static () => 0, (_, input) => action(input),
            cancellationToken: cancellationToken, completed: completed);

    internal static TResult[] Run<TInput, TState, TResult>(IReadOnlyList<TInput> inputs,
        Func<TState> createState, Func<TState, TInput, TResult> action,
        Action<TState>? disposeState = null, CancellationToken cancellationToken = default,
        Action<int>? completed = null)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (inputs.Count == 0)
            return [];
        TResult[] results = new TResult[inputs.Count];
        ExceptionDispatchInfo?[] errors = new ExceptionDispatchInfo?[inputs.Count];
        int next = -1;
        int failed = inputs.Count;
        int finished = 0;
        object progressLock = new();
        int workers = depth == 0 ? Math.Min(inputs.Count, Math.Max(1, Environment.ProcessorCount - 1)) : 1;

        void fail(int index, Exception exception)
        {
            errors[index] ??= ExceptionDispatchInfo.Capture(exception);
            int previous = Volatile.Read(ref failed);
            while (index < previous)
            {
                int observed = Interlocked.CompareExchange(ref failed, index, previous);
                if (observed == previous)
                    break;
                previous = observed;
            }
        }

        void work()
        {
            int index = Interlocked.Increment(ref next);
            if (index >= Volatile.Read(ref failed) || cancellationToken.IsCancellationRequested)
                return;
            int first = index;
            TState state = default!;
            bool created = false;
            depth++;
            try
            {
                state = createState();
                created = true;
                while (index < Volatile.Read(ref failed) && !cancellationToken.IsCancellationRequested)
                {
                    try
                    {
                        results[index] = action(state, inputs[index]);
                        if (completed is not null)
                            lock (progressLock)
                                completed(++finished);
                    }
                    catch (Exception exception)
                    {
                        fail(index, exception);
                        break;
                    }
                    index = Interlocked.Increment(ref next);
                }
            }
            catch (Exception exception)
            {
                fail(first, exception);
            }
            finally
            {
                try
                {
                    if (created)
                        disposeState?.Invoke(state);
                }
                catch (Exception exception)
                {
                    fail(first, exception);
                }
                depth--;
            }
        }

        if (workers == 1)
            work();
        else
            Parallel.For(0, workers, new ParallelOptions
            {
                MaxDegreeOfParallelism = workers,
                TaskScheduler = TaskScheduler.Default,
            }, _ => work());
        cancellationToken.ThrowIfCancellationRequested();
        foreach (ExceptionDispatchInfo? error in errors)
            error?.Throw();
        return results;
    }
}
