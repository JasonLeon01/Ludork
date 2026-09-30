export type DocManifestEntry = {
  key: string
  path: string
}

export type DocManifestNode = DocManifestEntry & {
  children?: DocManifestNode[]
}

export type DocManifestSection = DocManifestEntry & {
  children: DocManifestNode[]
}

export type DocsManifest = {
  home: DocManifestEntry
  sections: DocManifestSection[]
}
