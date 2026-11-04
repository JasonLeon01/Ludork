import { readFileSync, mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { relative, resolve, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import react from '@vitejs/plugin-react'
import sirv from 'sirv'
import { defineConfig, type Plugin } from 'vite'
import { getDocsRoute, sortDocsVersions } from '../docs_versions.mjs'
import { readDocsManifest } from './build/readDocsManifest.ts'
import { readDocsSearchIndexes } from './build/readDocsSearchIndexes.ts'

const VIRTUAL_MODULE_ID = 'virtual:ludork-doc-manifest'
const RESOLVED_VIRTUAL_MODULE_ID = `\0${VIRTUAL_MODULE_ID}`
const PROJECT_DIRECTORY = fileURLToPath(new URL('.', import.meta.url))
const SITE_DIRECTORY = resolve(PROJECT_DIRECTORY, '..')
const versions = sortDocsVersions(JSON.parse(readFileSync(resolve(SITE_DIRECTORY, 'versions.json'), 'utf8')))

function ludorkDocsPlugin(): Plugin {
  const contentRoute = (base: string, version: string) => `${base === './' ? '/' : base}${version}`
  return {
    name: 'ludork-local-docs',
    resolveId(id) { if (id === VIRTUAL_MODULE_ID) return RESOLVED_VIRTUAL_MODULE_ID },
    load(id) { if (id === RESOLVED_VIRTUAL_MODULE_ID) return `export default ${JSON.stringify(readDocsManifest(SITE_DIRECTORY, versions))}` },
    generateBundle() {
      const manifests = readDocsManifest(SITE_DIRECTORY, versions)
      for (const index of readDocsSearchIndexes(SITE_DIRECTORY, manifests)) {
        this.emitFile({
          type: 'asset',
          fileName: `${getDocsRoute(index.version)}search-index.${index.language}.json`,
          source: `${JSON.stringify(index)}\n`,
        })
      }
    },
    configureServer(server) {
      for (const version of versions) {
        const source = resolve(SITE_DIRECTORY, version)
        server.watcher.add(source)
        server.middlewares.use(contentRoute(server.config.base, version), sirv(source, { dev: true, etag: true }))
      }
    },
    configurePreviewServer(server) {
      for (const version of versions) {
        server.middlewares.use(contentRoute(server.config.base, version), sirv(resolve(SITE_DIRECTORY, version), { dev: true, etag: true }))
      }
    },
    handleHotUpdate(context) {
      const path = relative(SITE_DIRECTORY, context.file)
      if (!versions.some((version) => path.startsWith(`${version}${sep}`))) return
      const module = context.server.moduleGraph.getModuleById(RESOLVED_VIRTUAL_MODULE_ID)
      if (module) context.server.moduleGraph.invalidateModule(module)
      context.server.ws.send({ type: 'full-reload' })
      return []
    },
  }
}

const docsDirectory = resolve(PROJECT_DIRECTORY, 'docs')
const docsRoutes = new Set(versions.map(getDocsRoute))
for (const entry of readdirSync(docsDirectory, { withFileTypes: true })) {
  if (entry.isDirectory() && /^(?:trunk|v\d+\.\d+)$/.test(entry.name)
    && !docsRoutes.has(`docs/${entry.name}/`)) {
    rmSync(resolve(docsDirectory, entry.name), { recursive: true, force: true })
  }
}

const template = readFileSync(resolve(docsDirectory, 'index.html'), 'utf8')
const docsInputs = Object.fromEntries(versions.map((version) => {
  const route = getDocsRoute(version)
  const filename = resolve(PROJECT_DIRECTORY, route, 'index.html')
  mkdirSync(resolve(PROJECT_DIRECTORY, route), { recursive: true })
  writeFileSync(filename, template.replace('data-site-root="../"', 'data-site-root="../../"'))
  return [`docs-${version}`, filename]
}))

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
        ...docsInputs,
      },
    },
  },
})
