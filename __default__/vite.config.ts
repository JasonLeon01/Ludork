import { readdirSync, readFileSync, mkdirSync, writeFileSync } from 'node:fs'
import { relative, resolve, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import react from '@vitejs/plugin-react'
import sirv from 'sirv'
import { defineConfig, type Plugin } from 'vite'

const VIRTUAL_MODULE_ID = 'virtual:ludork-doc-filenames'
const RESOLVED_VIRTUAL_MODULE_ID = `\0${VIRTUAL_MODULE_ID}`
const PROJECT_DIRECTORY = fileURLToPath(new URL('.', import.meta.url))
const SITE_DIRECTORY = resolve(PROJECT_DIRECTORY, '..')
const versions: string[] = JSON.parse(readFileSync(resolve(SITE_DIRECTORY, 'versions.json'), 'utf8'))
const LANGUAGES = ['en_GB', 'zh_CN'] as const

function readMarkdownFiles(directory: string, prefix = ''): string[] {
  return readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const path = prefix ? `${prefix}/${entry.name}` : entry.name
    return entry.isDirectory() ? readMarkdownFiles(resolve(directory, entry.name), path)
      : entry.isFile() && entry.name.endsWith('.md') ? [path] : []
  }).sort()
}

function readDocsManifest() {
  return Object.fromEntries(versions.map((version) => [version, Object.fromEntries(
    LANGUAGES.map((language) => [language, readMarkdownFiles(resolve(SITE_DIRECTORY, version, 'docs', language))]),
  )]))
}

function ludorkDocsPlugin(): Plugin {
  return {
    name: 'ludork-local-docs',
    resolveId(id) { if (id === VIRTUAL_MODULE_ID) return RESOLVED_VIRTUAL_MODULE_ID },
    load(id) { if (id === RESOLVED_VIRTUAL_MODULE_ID) return `export default ${JSON.stringify(readDocsManifest())}` },
    configureServer(server) {
      for (const version of versions) {
        const source = resolve(SITE_DIRECTORY, version)
        server.watcher.add(source)
        server.middlewares.use(`/${version}`, sirv(source, { dev: true, etag: true }))
      }
    },
    configurePreviewServer(server) {
      for (const version of versions) {
        server.middlewares.use(`/${version}`, sirv(resolve(SITE_DIRECTORY, version), { dev: true, etag: true }))
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

const template = readFileSync(resolve(PROJECT_DIRECTORY, 'docs/index.html'), 'utf8')
const docsInputs = Object.fromEntries(versions.map((version) => {
  const filename = resolve(PROJECT_DIRECTORY, `docs/v${version}/index.html`)
  mkdirSync(resolve(PROJECT_DIRECTORY, `docs/v${version}`), { recursive: true })
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
