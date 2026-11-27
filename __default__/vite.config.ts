import { readFileSync } from 'node:fs'
import { relative, resolve, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import react from '@vitejs/plugin-react'
import sirv from 'sirv'
import { defineConfig, type Plugin } from 'vite'
import { getDocsRoute, sortDocsVersions } from '../docs_versions.mjs'
import { readDocsManifest } from './build/readDocsManifest.ts'
import { readDocsSearchIndexes } from './build/readDocsSearchIndexes.ts'

const VIRTUAL_VERSIONS_ID = 'virtual:ludork-versions'
const RESOLVED_VIRTUAL_VERSIONS_ID = `\0${VIRTUAL_VERSIONS_ID}`
const PROJECT_DIRECTORY = fileURLToPath(new URL('.', import.meta.url))
const SITE_DIRECTORY = resolve(PROJECT_DIRECTORY, '..')
const DOCS_TEMPLATE_PATH = resolve(PROJECT_DIRECTORY, 'docs/index.html')

function readSortedVersions(): string[] {
  return sortDocsVersions(JSON.parse(readFileSync(resolve(SITE_DIRECTORY, 'versions.json'), 'utf8')))
}

function toVersionedDocsHtml(compiledDocsHtml: string): string {
  const depthAdjusted = compiledDocsHtml
    .replace('data-site-root="../"', 'data-site-root="../../"')
    .replace(/(href|src)="\.\.\//g, '$1="../../')
  return depthAdjusted.replace(
    '</head>',
    '    <link rel="preload" as="fetch" href="./manifest.json" crossorigin>\n  </head>',
  )
}

function stripBasePrefix(pathname: string, base: string): string {
  const normalizedBase = base === './' ? '/' : base
  if (normalizedBase !== '/' && pathname.startsWith(normalizedBase)) {
    return pathname.slice(normalizedBase.length)
  }
  return pathname.replace(/^\/+/, '')
}

function resolveRouteVersion(segment: string, versions: readonly string[]): string | null {
  const version = segment.startsWith('v') ? segment.slice(1) : segment
  return versions.includes(version) ? version : null
}

function ludorkDocsPlugin(): Plugin {
  const contentRoute = (base: string, version: string) => `${base === './' ? '/' : base}${version}`
  return {
    name: 'ludork-local-docs',
    resolveId(id) {
      if (id === VIRTUAL_VERSIONS_ID) return RESOLVED_VIRTUAL_VERSIONS_ID
    },
    load(id) {
      if (id === RESOLVED_VIRTUAL_VERSIONS_ID) {
        return `export default ${JSON.stringify(readSortedVersions())}`
      }
    },
    generateBundle: {
      order: 'post',
      handler(_, bundle) {
        const versions = readSortedVersions()
        const docsHtmlAsset = bundle['docs/index.html']
        if (!docsHtmlAsset || docsHtmlAsset.type !== 'asset' || typeof docsHtmlAsset.source !== 'string') {
          throw new Error('Missing built docs/index.html template asset.')
        }
        const versionedHtml = toVersionedDocsHtml(docsHtmlAsset.source)
        delete bundle['docs/index.html']

        for (const version of versions) {
          this.emitFile({
            type: 'asset',
            fileName: `${getDocsRoute(version)}index.html`,
            source: versionedHtml,
          })
        }

        const manifests = readDocsManifest(SITE_DIRECTORY, versions)
        for (const [version, versionManifest] of Object.entries(manifests)) {
          this.emitFile({
            type: 'asset',
            fileName: `${getDocsRoute(version)}manifest.json`,
            source: `${JSON.stringify(versionManifest)}\n`,
          })
        }

        for (const index of readDocsSearchIndexes(SITE_DIRECTORY, manifests)) {
          this.emitFile({
            type: 'asset',
            fileName: `${getDocsRoute(index.version)}search-index.${index.language}.json`,
            source: `${JSON.stringify(index)}\n`,
          })
        }
      },
    },
    configureServer(server) {
      const versions = readSortedVersions()
      server.watcher.add(resolve(SITE_DIRECTORY, 'versions.json'))
      for (const version of versions) {
        const source = resolve(SITE_DIRECTORY, version)
        server.watcher.add(source)
        server.middlewares.use(contentRoute(server.config.base, version), sirv(source, { dev: true, etag: true }))
      }

      server.middlewares.use(async (req, res, next) => {
        if (!req.url) return next()
        const requestUrl = new URL(req.url, 'http://localhost')
        const routePath = stripBasePrefix(requestUrl.pathname, server.config.base)
        const activeVersions = readSortedVersions()

        const manifestMatch = routePath.match(/^docs\/(trunk|v\d+\.\d+)\/manifest\.json$/)
        if (manifestMatch) {
          const version = resolveRouteVersion(manifestMatch[1], activeVersions)
          if (version) {
            const manifest = readDocsManifest(SITE_DIRECTORY, [version])[version]
            res.setHeader('Content-Type', 'application/json; charset=utf-8')
            res.end(`${JSON.stringify(manifest)}\n`)
            return
          }
        }

        const searchMatch = routePath.match(/^docs\/(trunk|v\d+\.\d+)\/search-index\.([^/]+)\.json$/)
        if (searchMatch) {
          const version = resolveRouteVersion(searchMatch[1], activeVersions)
          if (version) {
            const manifests = readDocsManifest(SITE_DIRECTORY, [version])
            const found = readDocsSearchIndexes(SITE_DIRECTORY, manifests)
              .find((entry) => entry.language === searchMatch[2])
            if (found) {
              res.setHeader('Content-Type', 'application/json; charset=utf-8')
              res.end(`${JSON.stringify(found)}\n`)
              return
            }
          }
        }

        const docsPageMatch = routePath.match(/^docs\/(trunk|v\d+\.\d+)(\/(?:index\.html)?)?$/)
        if (docsPageMatch) {
          const version = resolveRouteVersion(docsPageMatch[1], activeVersions)
          if (version) {
            if (!docsPageMatch[2]) {
              res.statusCode = 302
              res.setHeader('Location', `${requestUrl.pathname}/${requestUrl.search}`)
              res.end()
              return
            }
            const template = readFileSync(DOCS_TEMPLATE_PATH, 'utf8')
              .replace('data-site-root="../"', 'data-site-root="../../"')
            const transformed = await server.transformIndexHtml(req.url, template)
            res.setHeader('Content-Type', 'text/html; charset=utf-8')
            res.end(transformed)
            return
          }
        }

        next()
      })
    },
    configurePreviewServer(server) {
      for (const version of readSortedVersions()) {
        server.middlewares.use(
          contentRoute(server.config.base, version),
          sirv(resolve(SITE_DIRECTORY, version), { dev: true, etag: true }),
        )
      }
    },
    handleHotUpdate(context) {
      const path = relative(SITE_DIRECTORY, context.file)
      const versions = readSortedVersions()
      if (path === 'versions.json') {
        const module = context.server.moduleGraph.getModuleById(RESOLVED_VIRTUAL_VERSIONS_ID)
        if (module) context.server.moduleGraph.invalidateModule(module)
        context.server.ws.send({ type: 'full-reload' })
        return []
      }
      if (!versions.some((version) => path.startsWith(`${version}${sep}`))) return
      context.server.ws.send({ type: 'full-reload' })
      return []
    },
  }
}

export default defineConfig({
  appType: 'mpa',
  base: './',
  plugins: [react(), ludorkDocsPlugin()],
  build: {
    rolldownOptions: {
      input: {
        home: resolve(PROJECT_DIRECTORY, 'index.html'),
        about: resolve(PROJECT_DIRECTORY, 'about/index.html'),
        notices: resolve(PROJECT_DIRECTORY, 'notices/index.html'),
        credits: resolve(PROJECT_DIRECTORY, 'credits/index.html'),
        docs: DOCS_TEMPLATE_PATH,
      },
    },
  },
})
