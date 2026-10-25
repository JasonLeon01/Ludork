using System;
using System.Collections.Generic;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

internal sealed class StableRandom
{
    private uint state;

    public StableRandom(uint seed)
    {
        state = seed == 0 ? 0xA341316Cu : seed;
    }

    public int Next(int exclusiveMaximum)
    {
        if (exclusiveMaximum <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(exclusiveMaximum));
        }
        uint value = state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state = value;
        return (int)(value % (uint)exclusiveMaximum);
    }

    public int NextInclusive(int minimum, int maximum)
    {
        if (maximum < minimum)
        {
            throw new ArgumentOutOfRangeException(nameof(maximum));
        }
        return minimum + Next(maximum - minimum + 1);
    }

    public void Shuffle<T>(IList<T> values)
    {
        for (int index = values.Count - 1; index > 0; index--)
        {
            int other = Next(index + 1);
            (values[index], values[other]) = (values[other], values[index]);
        }
    }
}
