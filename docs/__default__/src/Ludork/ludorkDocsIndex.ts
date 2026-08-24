import docFilenamesByLanguage from 'virtual:ludork-doc-filenames'
import { LUDORK_LANGUAGE_KEYS, type LanguageKey } from './ludorkLanguages'

export type DocEntry = {
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

function sectionDisplayName(name: string): string {
  const displayName = name.replace(/^\d+[.\s_-]*/, '')
  return displayName || name
}

function byNumericPrefix(a: string, b: string): number {
  return (parseInt(a, 10) || 0) - (parseInt(b, 10) || 0)
}

function normalizeDocPath(filename: string): string {
  return filename.replace(/\\/g, '/').replace(/^\/+/, '')
}

function compareTreeItems(a: DocTreeItem, b: DocTreeItem): number {
  const aName = a.type === 'folder' ? a.path.split('/').at(-1)! : a.entry.filename.split('/').at(-1)!
  const bName = b.type === 'folder' ? b.path.split('/').at(-1)! : b.entry.filename.split('/').at(-1)!
  const prefixDiff = byNumericPrefix(aName, bName)
  if (prefixDiff !== 0) {
    return prefixDiff
  }

  const aLabel = a.type === 'folder' ? a.displayName : a.entry.displayName
  const bLabel = b.type === 'folder' ? b.displayName : b.entry.displayName
  return aLabel.localeCompare(bLabel, undefined, { numeric: true, sensitivity: 'base' })
}

function sortTreeItems(items: DocTreeItem[]): void {
  items.sort(compareTreeItems)
  items.forEach((item) => {
    if (item.type === 'folder') {
      sortTreeItems(item.children)
    }
  })
}

function docsFromFilenames(filenames: readonly string[]): DocTreeItem[] {
  const root: DocTreeItem[] = []
  const folders = new Map<string, Extract<DocTreeItem, { type: 'folder' }>>()

  filenames
    .map(normalizeDocPath)
    .filter((filename) => filename.endsWith('.md') && !filename.includes('//'))
    .forEach((filename) => {
      const parts = filename.split('/').filter(Boolean)
      if (!parts.length) {
        return
      }

      let siblings = root
      let folderPath = ''

      parts.slice(0, -1).forEach((folderName) => {
        folderPath = folderPath ? `${folderPath}/${folderName}` : folderName
        let folder = folders.get(folderPath)
        if (!folder) {
          folder = {
            type: 'folder',
            path: folderPath,
            displayName: entryDisplayName(folderName),
            children: [],
          }
          folders.set(folderPath, folder)
          siblings.push(folder)
        }
        siblings = folder.children
      })

      const leafName = parts.at(-1)!
      siblings.push({
        type: 'doc',
        entry: {
          filename,
          displayName: entryDisplayName(leafName),
        },
      })
    })

  sortTreeItems(root)
  return root
}

function seqFromSegment(segment: string): string | null {
  const name = segment.replace(/\.md$/i, '')
  return name.match(/^(\d+)\./)?.[1] ?? null
}

export function docKeyFromFilename(filename: string): string {
  return filename
    .split('/')
    .filter(Boolean)
    .map((part) => seqFromSegment(part) ?? part)
    .join('/')
}

export function getDocsHomeFilename(language: LanguageKey): string {
  const filenames = docFilenamesByLanguage[language]
  if (!filenames) {
    throw new Error(`Missing local docs manifest for ${language}`)
  }

  const candidates = filenames
    .map(normalizeDocPath)
    .filter((filename) => !filename.includes('/') && seqFromSegment(filename) === '00')
  if (candidates.length !== 1) {
    throw new Error(`Expected exactly one top-level 00 Markdown document for ${language}`)
  }

  return candidates[0]
}

export function getDocsHomePath(language: LanguageKey): string {
  return `${language}/${getDocsHomeFilename(language)}`
}

export function resolveKnownDocPath(path: string): KnownDocPath | null {
  const normalized = normalizeDocPath(path)
  const language = LUDORK_LANGUAGE_KEYS.find((key) => normalized.startsWith(`${key}/`))
  if (!language) {
    return null
  }

  const filename = normalized.slice(language.length + 1)
  const knownFilename = docFilenamesByLanguage[language]
    ?.map(normalizeDocPath)
    .find((candidate) => candidate === filename)
  if (!knownFilename) {
    return null
  }

  return {
    language,
    filename: knownFilename,
    docKey: knownFilename === getDocsHomeFilename(language)
      ? null
      : docKeyFromFilename(knownFilename),
  }
}

export function getDocsSections(language: LanguageKey): DocSection[] {
  const filenames = docFilenamesByLanguage[language]
  if (!filenames) {
    throw new Error(`Missing local docs manifest for ${language}`)
  }

  const homeFilename = getDocsHomeFilename(language)
  const roots = docsFromFilenames(filenames)

  return roots.map((item) => {
    if (item.type === 'folder') {
      return {
        key: docKeyFromFilename(item.path),
        displayName: sectionDisplayName(item.displayName),
        includesHome: false,
        items: item.children,
      }
    }

    const isHome = normalizeDocPath(item.entry.filename) === homeFilename
    return {
      key: docKeyFromFilename(item.entry.filename),
      displayName: sectionDisplayName(item.entry.displayName),
      includesHome: isHome,
      items: isHome ? [] : [item],
    }
  })
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

export function resolveFilenameByDocKey(filenames: readonly string[], docKey: string): string | null {
  return filenames.find((filename) => docKeyFromFilename(filename) === docKey) ?? null
}

export function findSectionByFilename(
  sections: readonly DocSection[],
  filename: string,
): DocSection | null {
  const normalized = normalizeDocPath(filename)
  return sections.find((section) => flattenDocFilenames(section.items).includes(normalized)) ?? null
}
