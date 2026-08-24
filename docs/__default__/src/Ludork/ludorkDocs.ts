const DOCS_ROOT = new URL('https://ludork-docs.local/docs/')
const DOCS_RELATIVE_ROOT = import.meta.env.DEV ? '../docs/' : './'

export type DocsReference = {
  path: string
  search: string
  hash: string
}

function encodeDocsPath(path: string): string {
  return path.split('/').map((part) => encodeURIComponent(part).replace(/%2B/gi, '+')).join('/')
}

function decodeDocsPath(pathname: string): string {
  return pathname.split('/').map(decodeURIComponent).join('/')
}

function validateDocsPath(path: string): string {
  const normalized = path.replace(/\\/g, '/').replace(/^\.\//, '')
  const parts = normalized.split('/')
  if (!normalized || normalized.startsWith('/') || parts.some((part) => part === '..' || part === '.')) {
    throw new Error(`Invalid docs path: ${path}`)
  }
  return normalized
}

function hasUrlScheme(value: string): boolean {
  return /^[a-z][a-z\d+.-]*:/i.test(value) || value.startsWith('//')
}

export function getDocsUrl(path: string, search = '', hash = ''): string {
  const normalized = validateDocsPath(path)
  const baseUrl = new URL(DOCS_RELATIVE_ROOT, window.location.href)
  const url = new URL(encodeDocsPath(normalized), baseUrl)
  url.search = search
  url.hash = hash
  return url.href
}

export function resolveDocsReference(currentPath: string, href: string): DocsReference | null {
  if (!href || href.startsWith('/') || hasUrlScheme(href)) {
    return null
  }

  const currentUrl = new URL(encodeDocsPath(validateDocsPath(currentPath)), DOCS_ROOT)
  const resolved = new URL(href, currentUrl)
  if (resolved.origin !== DOCS_ROOT.origin || !resolved.pathname.startsWith(DOCS_ROOT.pathname)) {
    return null
  }

  return {
    path: decodeDocsPath(resolved.pathname.slice(DOCS_ROOT.pathname.length)),
    search: resolved.search,
    hash: resolved.hash,
  }
}
