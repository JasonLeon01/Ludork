using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class AssetDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection tilesetDocuments;
    private readonly EditorDocumentCollection autoTileDocuments;
    private readonly EditorDocumentCollection animationDocuments;
    private readonly EditorDocumentCollection particleDocuments;
    private readonly EditorDocumentCollection curveDocuments;
    private readonly EditorDocumentCollection textConfigDocuments;

    internal AssetDataService(ProjectDataStore store, EditorDocumentCollection tilesetDocuments, EditorDocumentCollection autoTileDocuments, EditorDocumentCollection animationDocuments, EditorDocumentCollection particleDocuments, EditorDocumentCollection curveDocuments, EditorDocumentCollection textConfigDocuments)
    {
        this.store = store;
        this.tilesetDocuments = tilesetDocuments;
        this.autoTileDocuments = autoTileDocuments;
        this.animationDocuments = animationDocuments;
        this.particleDocuments = particleDocuments;
        this.curveDocuments = curveDocuments;
        this.textConfigDocuments = textConfigDocuments;

    }

    public IReadOnlyDictionary<string, TilesetSnapshot> TilesetData => new DocumentSnapshotDictionary<TilesetSnapshot>(tilesetDocuments, value => new TilesetSnapshot(value));

    public IReadOnlyDictionary<string, TilesetSnapshot> AutoTileData => new DocumentSnapshotDictionary<TilesetSnapshot>(autoTileDocuments, value => new TilesetSnapshot(value));

    public IReadOnlyDictionary<string, AnimationSnapshot> AnimationsData => new DocumentSnapshotDictionary<AnimationSnapshot>(animationDocuments, value => new AnimationSnapshot(value));

    public IReadOnlyDictionary<string, ParticleSnapshot> ParticlesData => new DocumentSnapshotDictionary<ParticleSnapshot>(particleDocuments, value => new ParticleSnapshot(value));

    public IReadOnlyDictionary<string, CurveSnapshot> CurvesData => new DocumentSnapshotDictionary<CurveSnapshot>(curveDocuments, value => new CurveSnapshot(value));

    public IReadOnlyDictionary<string, TextConfigSnapshot> TextConfigsData => new DocumentSnapshotDictionary<TextConfigSnapshot>(textConfigDocuments, value => new TextConfigSnapshot(value));

}
