import type { LanguageKey } from '../i18n/languages'

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
  faq: DocManifestSection
  sections: DocManifestSection[]
}

export type VersionDocsManifest = Record<LanguageKey, DocsManifest>
