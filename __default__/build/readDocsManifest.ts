import { readdirSync, readFileSync, statSync } from 'node:fs'
import { resolve } from 'node:path'
import { LUDORK_LANGUAGE_KEYS } from '../src/Ludork/ludorkLanguages.ts'
import type { DocManifestEntry, DocManifestNode, DocsManifest } from '../src/Ludork/ludorkDocsManifest.ts'

type RootOrder = {
  home: DocManifestEntry
  sections: DocManifestEntry[]
}

function readMarkdownFiles(directory: string, prefix = ''): string[] {
  return readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const path = prefix ? `${prefix}/${entry.name}` : entry.name
    return entry.isDirectory() ? readMarkdownFiles(resolve(directory, entry.name), path)
      : entry.isFile() && entry.name.endsWith('.md') ? [path] : []
  })
}

function readLanguageManifest(directory: string): DocsManifest {
  const order: RootOrder = JSON.parse(readFileSync(resolve(directory, 'order.json'), 'utf8'))
  const keys = new Set<string>()
  const paths = new Set<string>()
  const documents = new Set<string>()

  function entry(node: DocManifestEntry, parent: string, folder: boolean): DocManifestEntry {
    if (!node.key || keys.has(node.key)) {
      throw new Error(`Duplicate or missing documentation key ${node.key} in ${directory}`)
    }
    if (!node.path || node.path === '.' || node.path === '..' || /[\\/]/.test(node.path)) {
      throw new Error(`Expected a file or directory name for ${node.key} in ${directory}`)
    }
    const path = parent ? `${parent}/${node.path}` : node.path
    if (paths.has(path)) throw new Error(`Duplicate documentation path ${path} in ${directory}`)
    const stat = statSync(resolve(directory, path))
    if (folder ? !stat.isDirectory() : !stat.isFile() || !path.endsWith('.md')) {
      throw new Error(`Invalid documentation ${folder ? 'directory' : 'file'} ${path} in ${directory}`)
    }
    keys.add(node.key)
    paths.add(path)
    if (!folder) documents.add(path)
    return { key: node.key, path }
  }

  function nodes(items: DocManifestNode[], parent: string): DocManifestNode[] {
    return items.map((node) => {
      const current = entry(node, parent, node.children !== undefined)
      return node.children === undefined ? current : { ...current, children: nodes(node.children, current.path) }
    })
  }

  const home = entry(order.home, '', false)
  const sections = order.sections.map((section) => {
    const current = entry(section, '', true)
    const chapter: { items: DocManifestNode[] } = JSON.parse(
      readFileSync(resolve(directory, current.path, 'order.json'), 'utf8'),
    )
    return { ...current, children: nodes(chapter.items, current.path) }
  })
  const missing = readMarkdownFiles(directory).filter((path) => !documents.has(path))
  if (missing.length) throw new Error(`Documents missing from order.json in ${directory}: ${missing.join(', ')}`)
  return { home, sections }
}

function structure(manifest: DocsManifest): string {
  function nodes(items: DocManifestNode[]): unknown[] {
    return items.map((node) => ({
      key: node.key,
      ...(node.children === undefined ? {} : { children: nodes(node.children) }),
    })).sort((a, b) => a.key.localeCompare(b.key))
  }
  return JSON.stringify({ home: manifest.home.key, sections: nodes(manifest.sections) })
}

export function readDocsManifest(siteDirectory: string, versions: readonly string[]) {
  return Object.fromEntries(versions.map((version) => {
    let reference: string | undefined
    const languages = LUDORK_LANGUAGE_KEYS.map((language) => {
      const manifest = readLanguageManifest(resolve(siteDirectory, version, 'docs', language))
      const shape = structure(manifest)
      if (reference !== undefined && reference !== shape) {
        throw new Error(`Documentation keys or hierarchy differ between languages in ${version}: ${language}`)
      }
      reference = shape
      return [language, manifest]
    })
    return [version, Object.fromEntries(languages)]
  }))
}
