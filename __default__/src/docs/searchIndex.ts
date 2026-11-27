import { getDocsRoute } from '../../../docs_versions.mjs'
import { getSiteRoot } from '../data/site'
import type { LanguageKey } from '../i18n/languages'

export type DocsSearchDocument = {
  key: string
  title: string
  path: string
  content: string
}

export type DocsSearchIndex = {
  version: string
  language: string
  documents: DocsSearchDocument[]
}

export type DocsSearchMatch = {
  document: DocsSearchDocument
  sectionLabel: string
  heading: string | null
  snippet: string
  score: number
}

export type HighlightSegment = {
  text: string
  match: boolean
}

const loadedIndexes = new Map<string, DocsSearchIndex>()
const pendingIndexes = new Map<string, Promise<DocsSearchIndex>>()

function cacheKey(version: string, language: LanguageKey): string {
  return `${version}:${language}`
}

export function getLoadedDocsSearchIndex(version: string, language: LanguageKey): DocsSearchIndex | null {
  return loadedIndexes.get(cacheKey(version, language)) ?? null
}

export function loadDocsSearchIndex(version: string, language: LanguageKey): Promise<DocsSearchIndex> {
  const key = cacheKey(version, language)
  const cached = loadedIndexes.get(key)
  if (cached) return Promise.resolve(cached)
  const pending = pendingIndexes.get(key)
  if (pending) return pending

  const url = new URL(`${getDocsRoute(version)}search-index.${encodeURIComponent(language)}.json`, getSiteRoot())
  const request = fetch(url)
    .then(async (response) => {
      if (!response.ok) throw new Error(`HTTP ${response.status}`)
      if (response.headers.get('content-type')?.includes('text/html')) {
        throw new Error('Unexpected HTML response')
      }
      const data = await response.json() as DocsSearchIndex
      loadedIndexes.set(key, data)
      return data
    })
    .finally(() => {
      pendingIndexes.delete(key)
    })

  pendingIndexes.set(key, request)
  return request
}

function stripMarkdownLine(line: string): string {
  return line
    .replace(/^\s*#{1,6}\s+/, '')
    .replace(/^\s*>\s*/, '')
    .replace(/^\s*[-*+]\s+/, '')
    .replace(/!\[([^\]]*)\]\([^)]*\)/g, '$1')
    .replace(/\[([^\]]+)\]\([^)]*\)/g, '$1')
    .replace(/`{1,3}([^`]+)`{1,3}/g, '$1')
    .replace(/[*_~]+/g, '')
    .replace(/\|/g, ' ')
    .replace(/\s+/g, ' ')
    .trim()
}

function extractSectionLabel(path: string, language: LanguageKey): string {
  const prefix = `docs/${language}/`
  const relative = path.startsWith(prefix) ? path.slice(prefix.length) : path
  const parts = relative.split('/')
  if (parts.length <= 1) return ''
  return parts.slice(0, -1).join(' / ')
}

function buildSnippetAndHeading(content: string, terms: readonly string[]): { heading: string | null; snippet: string } {
  const lines = content.split('\n')
  let inCodeFence = false
  let currentHeading: string | null = null
  let bestHeading: string | null = null
  let bestLine = ''
  let bestLineScore = -1
  let fallbackLine = ''

  for (const rawLine of lines) {
    const trimmed = rawLine.trim()
    if (/^(`{3,}|~{3,})/.test(trimmed)) {
      inCodeFence = !inCodeFence
      continue
    }
    if (!inCodeFence) {
      const headingMatch = trimmed.match(/^#{2,4}\s+(.+)$/)
      if (headingMatch) {
        currentHeading = stripMarkdownLine(headingMatch[1])
      }
    }
    const clean = inCodeFence ? trimmed.replace(/\s+/g, ' ') : stripMarkdownLine(rawLine)
    if (!clean) continue
    if (!fallbackLine && !trimmed.startsWith('#')) {
      fallbackLine = clean
    }

    const lower = clean.toLocaleLowerCase()
    let hits = 0
    for (const term of terms) {
      if (lower.includes(term)) hits++
    }
    if (hits > 0) {
      const score = hits * 10 + (inCodeFence ? 0 : 2)
      if (score > bestLineScore) {
        bestLineScore = score
        bestLine = clean
        bestHeading = currentHeading
      }
    }
  }

  const source = bestLine || fallbackLine
  if (!source) return { heading: bestHeading, snippet: '' }

  const maxLength = 160
  if (source.length <= maxLength) {
    return { heading: bestHeading, snippet: source }
  }

  const lower = source.toLocaleLowerCase()
  let firstMatch = -1
  for (const term of terms) {
    const idx = lower.indexOf(term)
    if (idx !== -1 && (firstMatch === -1 || idx < firstMatch)) {
      firstMatch = idx
    }
  }
  const start = firstMatch > 48 ? Math.max(0, firstMatch - 48) : 0
  const end = Math.min(source.length, start + maxLength)
  const prefix = start > 0 ? '…' : ''
  const suffix = end < source.length ? '…' : ''
  return {
    heading: bestHeading,
    snippet: `${prefix}${source.slice(start, end).trim()}${suffix}`,
  }
}

export function tokenizeSearchQuery(rawQuery: string): string[] {
  return rawQuery
    .trim()
    .toLocaleLowerCase()
    .split(/\s+/)
    .filter(Boolean)
}

export function searchDocsIndex(
  index: DocsSearchIndex,
  language: LanguageKey,
  rawQuery: string,
  limit = 24,
): DocsSearchMatch[] {
  const terms = tokenizeSearchQuery(rawQuery)
  if (!terms.length) return []

  const fullQuery = terms.join(' ')
  const results: DocsSearchMatch[] = []

  for (const document of index.documents) {
    const titleLower = document.title.toLocaleLowerCase()
    const contentLower = document.content.toLocaleLowerCase()

    let matchesAll = true
    let titleMatches = 0
    let bodyMatches = 0

    for (const term of terms) {
      const inTitle = titleLower.includes(term)
      const inBody = contentLower.includes(term)
      if (!inTitle && !inBody) {
        matchesAll = false
        break
      }
      if (inTitle) titleMatches++
      if (inBody) bodyMatches++
    }
    if (!matchesAll) continue

    let score = titleMatches * 40 + bodyMatches * 10
    if (titleLower === fullQuery) score += 200
    else if (titleLower.startsWith(fullQuery)) score += 120
    else if (titleLower.includes(fullQuery)) score += 80
    else if (contentLower.includes(fullQuery)) score += 30

    const { heading, snippet } = buildSnippetAndHeading(document.content, terms)
    if (heading) {
      const headingLower = heading.toLocaleLowerCase()
      if (terms.some((term) => headingLower.includes(term))) score += 25
    }

    results.push({
      document,
      sectionLabel: extractSectionLabel(document.path, language),
      heading,
      snippet,
      score,
    })
  }

  results.sort((left, right) => right.score - left.score)
  return results.slice(0, limit)
}

export function splitHighlightSegments(text: string, rawQuery: string): HighlightSegment[] {
  const terms = tokenizeSearchQuery(rawQuery)
  if (!text || !terms.length) return [{ text, match: false }]

  const lower = text.toLocaleLowerCase()
  const ranges: Array<[number, number]> = []
  for (const term of terms) {
    let offset = 0
    while (offset < lower.length) {
      const index = lower.indexOf(term, offset)
      if (index === -1) break
      ranges.push([index, index + term.length])
      offset = index + term.length
    }
  }
  if (!ranges.length) return [{ text, match: false }]

  ranges.sort((a, b) => a[0] - b[0] || b[1] - a[1])
  const merged: Array<[number, number]> = []
  for (const [start, end] of ranges) {
    const previous = merged[merged.length - 1]
    if (previous && start <= previous[1]) {
      previous[1] = Math.max(previous[1], end)
    } else {
      merged.push([start, end])
    }
  }

  const segments: HighlightSegment[] = []
  let cursor = 0
  for (const [start, end] of merged) {
    if (start > cursor) segments.push({ text: text.slice(cursor, start), match: false })
    segments.push({ text: text.slice(start, end), match: true })
    cursor = end
  }
  if (cursor < text.length) segments.push({ text: text.slice(cursor), match: false })
  return segments
}
