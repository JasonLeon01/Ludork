import { getDocsRoute } from '../../../docs_versions.mjs'
import { getDocsVersion, isEmbedded } from '../docs/docsVersions'
import type { LanguageKey } from '../i18n/languages'

export type SitePage = 'home' | 'docs' | 'about' | 'notices' | 'credits'

export const LUDORK_LINKS = {
  website: 'https://jasonleon01.github.io/Ludork/',
  repository: 'https://github.com/JasonLeon01/Ludork',
  releases: 'https://github.com/JasonLeon01/Ludork/releases',
  issues: 'https://github.com/JasonLeon01/Ludork/issues',
  license: 'https://github.com/JasonLeon01/Ludork/blob/main/LICENSE.md',
} as const

export function getSiteRoot(): URL {
  return new URL(document.documentElement.dataset.siteRoot!, window.location.href)
}

export function getSiteAssetUrl(path: string): string {
  return new URL(path, getSiteRoot()).href
}

export function preserveEmbeddedMode(href: string | undefined): string | undefined {
  if (!href || !isEmbedded()) return href
  const url = new URL(href, getSiteRoot())
  const roots = [getSiteRoot(), new URL(LUDORK_LINKS.website)]
  const root = roots.find((candidate) => url.origin === candidate.origin && url.pathname.startsWith(candidate.pathname))
  if (root && /^(?:docs\/(?:trunk|v\d+\.\d+)|about|notices|credits)\/?$/.test(url.pathname.slice(root.pathname.length))) {
    url.searchParams.set('embedded', '1')
    return url.href
  }
  return href
}

export function getSitePageHref(page: SitePage, language: LanguageKey): string {
  const route = page === 'home' ? './' : page === 'docs' ? getDocsRoute(getDocsVersion()) : `${page}/`
  const url = new URL(route, getSiteRoot())
  if (page === 'about' || page === 'notices') url.searchParams.set('version', getDocsVersion())
  if (page !== 'home' && isEmbedded()) url.searchParams.set('embedded', '1')
  url.searchParams.set('lang', language)
  return `${url.pathname}${url.search}`
}
