import { readdirSync, readFileSync } from 'node:fs'
import type { IncomingMessage, ServerResponse } from 'node:http'
import { relative, resolve, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import react from '@vitejs/plugin-react'
import sirv from 'sirv'
import { defineConfig, type Plugin } from 'vite'

const VIRTUAL_MODULE_ID = 'virtual:ludork-doc-filenames'
const RESOLVED_VIRTUAL_MODULE_ID = `\0${VIRTUAL_MODULE_ID}`
const PROJECT_DIRECTORY = fileURLToPath(new URL('.', import.meta.url))
const DOCS_DIRECTORY = resolve(PROJECT_DIRECTORY, '..')
const LANGUAGE_DIRECTORIES = ['en_GB', 'zh_CN'] as const
const PUBLIC_DOCS_DIRECTORIES = ['_images', ...LANGUAGE_DIRECTORIES] as const
const ABOUT_FILES = LANGUAGE_DIRECTORIES.map((language) => `About_${language}.md`)

type DocsManifest = Record<(typeof LANGUAGE_DIRECTORIES)[number], string[]>

function readMarkdownFiles(directory: string, prefix = ''): string[] {
  const filenames: string[] = []

  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const relativePath = prefix ? `${prefix}/${entry.name}` : entry.name
    const absolutePath = resolve(directory, entry.name)

    if (entry.isDirectory()) {
      filenames.push(...readMarkdownFiles(absolutePath, relativePath))
    } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.md')) {
      filenames.push(relativePath)
    }
  }

  return filenames
}

function readDocsManifest(): DocsManifest {
  return Object.fromEntries(
    LANGUAGE_DIRECTORIES.map((language) => {
      const filenames = readMarkdownFiles(resolve(DOCS_DIRECTORY, language))
      if (!filenames.length) {
        throw new Error(`No Markdown files found in docs/${language}`)
      }
      return [language, filenames]
    }),
  ) as DocsManifest
}

function isDocsFile(filename: string): boolean {
  const relativePath = relative(DOCS_DIRECTORY, filename)
  return ABOUT_FILES.includes(relativePath) || PUBLIC_DOCS_DIRECTORIES.some((directory) =>
    relativePath === directory || relativePath.startsWith(`${directory}${sep}`),
  )
}

function serveAbout(request: IncomingMessage, response: ServerResponse, next: () => void): void {
  const filename = request.url?.split('?')[0].replace(/^\//, '') ?? ''
  if (!ABOUT_FILES.includes(filename)) {
    next()
    return
  }
  try {
    const content = readFileSync(resolve(DOCS_DIRECTORY, filename))
    response.setHeader('Content-Type', 'text/markdown; charset=utf-8')
    response.end(content)
  } catch {
    response.statusCode = 404
    response.end('Not found')
  }
}

function ludorkDocsPlugin(): Plugin {
  return {
    name: 'ludork-local-docs',
    resolveId(id) {
      if (id === VIRTUAL_MODULE_ID) {
        return RESOLVED_VIRTUAL_MODULE_ID
      }
    },
    load(id) {
      if (id !== RESOLVED_VIRTUAL_MODULE_ID) {
        return
      }

      return `export default ${JSON.stringify(readDocsManifest())}`
    },
    configureServer(server) {
      server.middlewares.use(serveAbout)
      server.watcher.add(ABOUT_FILES.map((filename) => resolve(DOCS_DIRECTORY, filename)))
      for (const directory of PUBLIC_DOCS_DIRECTORIES) {
        const source = resolve(DOCS_DIRECTORY, directory)
        server.watcher.add(source)
        server.middlewares.use(`/${directory}`, sirv(source, { dev: true, etag: true }))
      }
    },
    configurePreviewServer(server) {
      server.middlewares.use(serveAbout)
      for (const directory of PUBLIC_DOCS_DIRECTORIES) {
        server.middlewares.use(
          `/${directory}`,
          sirv(resolve(DOCS_DIRECTORY, directory), { dev: true, etag: true }),
        )
      }
    },
    handleHotUpdate(context) {
      if (!isDocsFile(context.file)) {
        return
      }

      const module = context.server.moduleGraph.getModuleById(RESOLVED_VIRTUAL_MODULE_ID)
      if (module) {
        context.server.moduleGraph.invalidateModule(module)
      }
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
        docs: resolve(PROJECT_DIRECTORY, 'docs/index.html'),
        about: resolve(PROJECT_DIRECTORY, 'about/index.html'),
      },
    },
  },
})
