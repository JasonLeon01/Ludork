import { readFileSync } from 'node:fs'
import { basename, resolve } from 'node:path'
import type { DocManifestEntry, DocManifestNode, DocsManifest } from '../src/Ludork/ludorkDocsManifest.ts'

type DocsSearchDocument = {
  key: string
  title: string
  path: string
  content: string
}

type DocsSearchIndex = {
  version: string
  language: string
  documents: DocsSearchDocument[]
}

function documentEntries(nodes: DocManifestNode[]): DocManifestEntry[] {
  return nodes.flatMap((node) => node.children === undefined ? [node] : documentEntries(node.children))
}

export function readDocsSearchIndexes(
  siteDirectory: string,
  manifests: Record<string, Record<string, DocsManifest>>,
): DocsSearchIndex[] {
  return Object.entries(manifests).flatMap(([version, languages]) =>
    Object.entries(languages).map(([language, manifest]) => ({
      version,
      language,
      documents: [manifest.home, ...documentEntries([manifest.faq, ...manifest.sections])].map((entry) => {
        const path = `docs/${language}/${entry.path}`
        return {
          key: entry.key,
          title: basename(entry.path).replace(/\.md$/i, ''),
          path,
          content: readFileSync(resolve(siteDirectory, version, path), 'utf8').replace(/\r\n?/g, '\n'),
        }
      }),
    })),
  )
}
