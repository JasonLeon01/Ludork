using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public static class RandomMapGenerator
{
    public static RandomMapDensityProfile GetDensityProfile(RandomMapMode mode)
    {
        return mode switch
        {
            RandomMapMode.Lattice => new RandomMapDensityProfile(20, 35, 30),
            RandomMapMode.Rooms => new RandomMapDensityProfile(30, 45, 36),
            _ => throw new ArgumentOutOfRangeException(nameof(mode)),
        };
    }

    public static RandomMapGenerationResult Generate(
        int width,
        int height,
        int wallTile,
        IEnumerable<RandomMapPoint> markers,
        RandomMapMode mode,
        int seed,
        CancellationToken cancellationToken = default)
    {
        int density = mode is RandomMapMode.Lattice or RandomMapMode.Rooms
            ? GetDensityProfile(mode).Default
            : 0;
        return Generate(width, height, wallTile, markers, mode, density, seed, cancellationToken);
    }

    public static RandomMapGenerationResult Generate(
        int width,
        int height,
        int wallTile,
        IEnumerable<RandomMapPoint> markers,
        RandomMapMode mode,
        int densityPercent,
        int seed,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string? error = ValidateInput(width, height, markers, mode, densityPercent, cancellationToken, out HashSet<RandomMapPoint> markerSet);
        if (error is not null)
        {
            return Failure(error, seed);
        }

        int area = width * height;
        int targetWalls = (int)(((long)area * densityPercent + 50) / 100);
        int tolerance = Math.Max(1, (int)(((long)area * 2 + 99) / 100));
        if (area - markerSet.Count < targetWalls - tolerance)
        {
            return Failure("noSafeDensity", seed);
        }

        MapLayout? best = null;
        double bestScore = double.NegativeInfinity;
        int candidateCount = area < 400 ? 64 : area < 1024 ? 32 : 16;
        for (int index = 0; index < candidateCount; index++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            StableRandom random = new StableRandom(DeriveSeed(seed, index));
            MapLayout? candidate = mode == RandomMapMode.Lattice
                ? LatticeLayoutGenerator.Generate(width, height, markerSet, targetWalls, tolerance, random, cancellationToken)
                : RoomLayoutGenerator.Generate(width, height, markerSet, targetWalls, tolerance, random, cancellationToken);
            if (candidate is null)
            {
                continue;
            }

            int distance = Math.Abs(MapTopology.CountWalls(candidate.Walls) - targetWalls);
            if (distance > tolerance
                || markerSet.Any(point => candidate.Walls[point.Y, point.X])
                || MapTopology.HasWallBlock(candidate.Walls)
                || !MapTopology.IsConnected(candidate.Walls, cancellationToken))
            {
                continue;
            }

            double score = candidate.Quality - distance / (double)area;
            if (score > bestScore)
            {
                best = candidate;
                bestScore = score;
            }
        }

        cancellationToken.ThrowIfCancellationRequested();
        if (best is null)
        {
            return Failure(mode == RandomMapMode.Lattice ? "latticeStyleFailed" : "noRoomLayout", seed);
        }

        IReadOnlyList<int?>[] tiles = new IReadOnlyList<int?>[height];
        for (int y = 0; y < height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            int?[] row = new int?[width];
            for (int x = 0; x < width; x++)
            {
                row[x] = best.Walls[y, x] ? wallTile : null;
            }
            tiles[y] = row;
        }
        return new RandomMapGenerationResult(true, string.Empty, tiles, seed);
    }

    private static string? ValidateInput(
        int width,
        int height,
        IEnumerable<RandomMapPoint> markers,
        RandomMapMode mode,
        int density,
        CancellationToken cancellationToken,
        out HashSet<RandomMapPoint> markerSet)
    {
        markerSet = [];
        if (width <= 0 || height <= 0 || (long)width * height > int.MaxValue)
        {
            return "invalidDimensions";
        }
        if (markers is null)
        {
            return "invalidMarkers";
        }
        if (mode is not RandomMapMode.Lattice and not RandomMapMode.Rooms)
        {
            return "invalidMode";
        }
        RandomMapDensityProfile profile = GetDensityProfile(mode);
        if (density < profile.Minimum || density > profile.Maximum)
        {
            return "invalidDensity";
        }
        if (mode == RandomMapMode.Lattice && (width < 7 || height < 7))
        {
            return "latticeTooSmall";
        }
        if (mode == RandomMapMode.Rooms && (Math.Min(width, height) < 7 || (long)width * height < 81))
        {
            return "roomsTooSmall";
        }
        foreach (RandomMapPoint marker in markers)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (marker.X < 0 || marker.Y < 0 || marker.X >= width || marker.Y >= height)
            {
                return "markerOutOfBounds";
            }
            markerSet.Add(marker);
        }
        return null;
    }

    private static RandomMapGenerationResult Failure(string error, int seed)
    {
        return new RandomMapGenerationResult(false, error, Array.Empty<IReadOnlyList<int?>>(), seed);
    }

    private static uint DeriveSeed(int seed, int index)
    {
        uint value = unchecked((uint)seed) + 0x9E3779B9u * unchecked((uint)(index + 1));
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        value *= 0x846CA68Bu;
        return value ^ (value >> 16);
    }
}
