import { getDocsRoute } from '../../../docs_versions.mjs'
import { getSiteRoot } from '../data/site'
import { LUDORK_LANGUAGE_KEYS, type LanguageKey } from '../i18n/languages'
import type { DocManifestNode, DocsManifest, VersionDocsManifest } from './docsManifest'
import { getDocsVersion } from './docsVersions'

export type DocEntry = {
  key: string
  filename: string
  displayName: string
}

export type DocTreeItem =
  | { type: 'folder'; key: string; displayName: string; children: DocTreeItem[] }
  | { type: 'doc'; entry: DocEntry }

export type DocSection = {
  key: string
  displayName: string
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

export type DocNavLink = {
  selected: SelectedDoc
  title: string
  sectionTitle?: string
}

export type AdjacentDocs = {
  previous: DocNavLink | null
  next: DocNavLink | null
}

const loadedManifests = new Map<string, VersionDocsManifest>()
const pendingManifests = new Map<string, Promise<VersionDocsManifest>>()

export function getLoadedDocsManifest(version = getDocsVersion()): VersionDocsManifest | null {
  return loadedManifests.get(version) ?? null
}

export function loadDocsManifest(version = getDocsVersion()): Promise<VersionDocsManifest> {
  const cached = loadedManifests.get(version)
  if (cached) return Promise.resolve(cached)
  const pending = pendingManifests.get(version)
  if (pending) return pending

  const url = new URL(`${getDocsRoute(version)}manifest.json`, getSiteRoot())
  const request = fetch(url)
    .then(async (response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`)
      if (response.headers.get('content-type')?.includes('text/html')) {
        throw new Error('Unexpected HTML response')
      }
      const data = await response.json() as VersionDocsManifest
      loadedManifests.set(version, data)
      return data
    })
    .finally(() => {
      pendingManifests.delete(version)
    })

  pendingManifests.set(version, request)
  return request
}

function entryDisplayName(filename: string): string {
  return filename.replace(/\.md$/i, '')
}

function normalizeDocPath(filename: string): string {
  return filename.replace(/\\/g, '/').replace(/^\/+/, '')
}

function getLanguageManifest(manifests: VersionDocsManifest, language: LanguageKey): DocsManifest {
  const manifest = manifests[language]
  if (!manifest) throw new Error(`Missing docs manifest for ${language}`)
  return manifest
}

function docsFromManifest(nodes: DocManifestNode[]): DocTreeItem[] {
  return nodes.map((node) => {
    const displayName = entryDisplayName(node.path.split('/').at(-1)!)
    return node.children === undefined
      ? { type: 'doc', entry: { key: node.key, filename: node.path, displayName } }
      : { type: 'folder', key: node.key, displayName, children: docsFromManifest(node.children) }
  })
}

function flattenManifest(nodes: DocManifestNode[]): DocManifestNode[] {
  return nodes.flatMap((node) => node.children === undefined ? [node] : flattenManifest(node.children))
}

export function getDocKeyByFilename(manifests: VersionDocsManifest, language: LanguageKey, filename: string): string | null {
  const manifest = getLanguageManifest(manifests, language)
  return flattenManifest([manifest.faq, ...manifest.sections])
    .find((node) => node.path === normalizeDocPath(filename))?.key ?? null
}

export function getDocsHomeFilename(manifests: VersionDocsManifest, language: LanguageKey): string {
  return getLanguageManifest(manifests, language).home.path
}

export function getDocsHomePath(manifests: VersionDocsManifest, language: LanguageKey): string {
  return `docs/${language}/${getDocsHomeFilename(manifests, language)}`
}

export function getDocsFaqTree(manifests: VersionDocsManifest, language: LanguageKey): Extract<DocTreeItem, { type: 'folder' }> {
  const faq = getLanguageManifest(manifests, language).faq
  return { type: 'folder', key: faq.key, displayName: faq.path, children: docsFromManifest(faq.children) }
}

export function isFaqDoc(manifests: VersionDocsManifest, language: LanguageKey, docKey: string): boolean {
  return flattenManifest(getLanguageManifest(manifests, language).faq.children).some((node) => node.key === docKey)
}

export function resolveKnownDocPath(manifests: VersionDocsManifest, path: string): KnownDocPath | null {
  const normalized = normalizeDocPath(path).replace(/^docs\//, '')
  const language = LUDORK_LANGUAGE_KEYS.find((key) => normalized.startsWith(`${key}/`))
  if (!language) {
    return null
  }

  const filename = normalized.slice(language.length + 1)
  const isHome = filename === getDocsHomeFilename(manifests, language)
  const docKey = isHome ? null : getDocKeyByFilename(manifests, language, filename)
  return isHome || docKey ? { language, filename, docKey } : null
}

export function getDocsSections(manifests: VersionDocsManifest, language: LanguageKey): DocSection[] {
  const manifest = getLanguageManifest(manifests, language)
  return manifest.sections.map((section) => ({
    key: section.key,
    displayName: section.path,
    items: docsFromManifest(section.children),
  }))
}

export function resolveFilenameByDocKey(manifests: VersionDocsManifest, language: LanguageKey, docKey: string): string | null {
  const manifest = getLanguageManifest(manifests, language)
  return flattenManifest([manifest.faq, ...manifest.sections]).find((node) => node.key === docKey)?.path ?? null
}

export function getDocsHomeKey(manifests: VersionDocsManifest, language: LanguageKey): string {
  return getLanguageManifest(manifests, language).home.key
}

function findBreadcrumbTrail(nodes: DocManifestNode[], docKey: string): string[] | null {
  for (const node of nodes) {
    const label = entryDisplayName(node.path.split('/').at(-1)!)
    if (node.children === undefined) {
      if (node.key === docKey) return [label]
    } else {
      const childTrail = findBreadcrumbTrail(node.children, docKey)
      if (childTrail) return [label, ...childTrail]
    }
  }
  return null
}

export function getDocBreadcrumbs(
  manifests: VersionDocsManifest,
  language: LanguageKey,
  selected: SelectedDoc,
  faqLabel: string,
): string[] {
  if (selected.type !== 'doc') return []
  const manifest = getLanguageManifest(manifests, language)
  const faqTrail = findBreadcrumbTrail(manifest.faq.children, selected.docKey)
  if (faqTrail) return [faqLabel, ...faqTrail]
  return findBreadcrumbTrail(manifest.sections, selected.docKey) ?? []
}

function collectSectionNavLinks(
  nodes: DocManifestNode[],
  language: LanguageKey,
  sectionTitle: string,
): DocNavLink[] {
  return nodes.flatMap((node) => {
    if (node.children !== undefined) {
      return collectSectionNavLinks(node.children, language, sectionTitle)
    }
    return [{
      selected: { type: 'doc', lang: language, docKey: node.key },
      title: entryDisplayName(node.path.split('/').at(-1)!),
      sectionTitle,
    }]
  })
}

export function getAdjacentDocs(
  manifests: VersionDocsManifest,
  language: LanguageKey,
  selected: SelectedDoc,
  homeTitle: string,
  faqLabel: string,
): AdjacentDocs {
  const manifest = getLanguageManifest(manifests, language)
  const ordered: DocNavLink[] = [
    { selected: { type: 'home' }, title: homeTitle },
    ...collectSectionNavLinks(manifest.faq.children, language, faqLabel),
    ...manifest.sections.flatMap((section) =>
      collectSectionNavLinks(section.children, language, section.path),
    ),
  ]

  const currentIndex = ordered.findIndex((item) =>
    selected.type === 'home'
      ? item.selected.type === 'home'
      : item.selected.type === 'doc' && item.selected.docKey === selected.docKey,
  )
  if (currentIndex === -1) return { previous: null, next: null }

  return {
    previous: currentIndex > 0 ? ordered[currentIndex - 1] : null,
    next: currentIndex < ordered.length - 1 ? ordered[currentIndex + 1] : null,
  }
}
