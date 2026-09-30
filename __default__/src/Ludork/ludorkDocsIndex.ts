import { getDocsVersion } from './ludorkVersions'
import manifestsByVersion from 'virtual:ludork-doc-manifest'
import type { DocManifestNode, DocsManifest } from './ludorkDocsManifest'
import { LUDORK_LANGUAGE_KEYS, type LanguageKey } from './ludorkLanguages'

export type DocEntry = {
  key: string
  filename: string
  displayName: string
}

export type DocTreeItem =
  | { type: 'folder'; path: string; displayName: string; children: DocTreeItem[] }
  | { type: 'doc'; entry: DocEntry }

export type DocSection = {
  key: string
  displayName: string
  includesHome: boolean
  items: DocTreeItem[]
}

export type SelectedDoc =
  | { type: 'home' }
  | { type: 'doc'; lang: LanguageKey; docKey: string }

export type KnownDocPath = {
  language: LanguageKey
  filename: string
  docKey: string | null
}

function entryDisplayName(filename: string): string {
  return filename.replace(/\.md$/i, '')
}

function normalizeDocPath(filename: string): string {
  return filename.replace(/\\/g, '/').replace(/^\/+/, '')
}

function getManifest(language: LanguageKey): DocsManifest {
  const manifest = manifestsByVersion[getDocsVersion()]?.[language]
  if (!manifest) throw new Error(`Missing local docs manifest for ${language}`)
  return manifest
}

function docsFromManifest(nodes: DocManifestNode[]): DocTreeItem[] {
  return nodes.map((node) => {
    const displayName = entryDisplayName(node.path.split('/').at(-1)!)
    return node.children === undefined
      ? { type: 'doc', entry: { key: node.key, filename: node.path, displayName } }
      : { type: 'folder', path: node.path, displayName, children: docsFromManifest(node.children) }
  })
}

function flattenManifest(nodes: DocManifestNode[]): DocManifestNode[] {
  return nodes.flatMap((node) => node.children === undefined ? [node] : flattenManifest(node.children))
}

export function getDocKeyByFilename(language: LanguageKey, filename: string): string | null {
  return flattenManifest(getManifest(language).sections)
    .find((node) => node.path === normalizeDocPath(filename))?.key ?? null
}

export function getDocsHomeFilename(language: LanguageKey): string {
  return getManifest(language).home.path
}

export function getDocsHomePath(language: LanguageKey): string {
  return `docs/${language}/${getDocsHomeFilename(language)}`
}

export function resolveKnownDocPath(path: string): KnownDocPath | null {
  const normalized = normalizeDocPath(path).replace(/^docs\//, '')
  const language = LUDORK_LANGUAGE_KEYS.find((key) => normalized.startsWith(`${key}/`))
  if (!language) {
    return null
  }

  const filename = normalized.slice(language.length + 1)
  const isHome = filename === getDocsHomeFilename(language)
  const docKey = isHome ? null : getDocKeyByFilename(language, filename)
  return isHome || docKey ? { language, filename, docKey } : null
}

export function getDocsSections(language: LanguageKey): DocSection[] {
  const manifest = getManifest(language)
  return [
    {
      key: manifest.home.key,
      displayName: entryDisplayName(manifest.home.path),
      includesHome: true,
      items: [],
    },
    ...manifest.sections.map((section) => ({
      key: section.key,
      displayName: section.path,
      includesHome: false,
      items: docsFromManifest(section.children),
    })),
  ]
}

export function flattenDocFilenames(items: readonly DocTreeItem[]): string[] {
  const filenames: string[] = []
  for (const item of items) {
    if (item.type === 'doc') {
      filenames.push(item.entry.filename)
    } else {
      filenames.push(...flattenDocFilenames(item.children))
    }
  }
  return filenames
}

export function resolveFilenameByDocKey(language: LanguageKey, docKey: string): string | null {
  return flattenManifest(getManifest(language).sections).find((node) => node.key === docKey)?.path ?? null
}

export function findSectionByFilename(
  sections: readonly DocSection[],
  filename: string,
): DocSection | null {
  const normalized = normalizeDocPath(filename)
  return sections.find((section) => flattenDocFilenames(section.items).includes(normalized)) ?? null
}
