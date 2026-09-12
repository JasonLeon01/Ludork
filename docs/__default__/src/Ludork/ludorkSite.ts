import type { LanguageKey } from './ludorkLanguages'

export type SitePage = 'home' | 'docs' | 'about' | 'notices'

export const LUDORK_LINKS = {
  repository: 'https://github.com/JasonLeon01/Ludork',
  releases: 'https://github.com/JasonLeon01/Ludork/releases',
  issues: 'https://github.com/JasonLeon01/Ludork/issues',
  license: 'https://github.com/JasonLeon01/Ludork/blob/main/LICENSE.md',
} as const

export function getSitePage(): SitePage {
  const page = document.documentElement.dataset.page
  if (page === 'home' || page === 'docs' || page === 'about' || page === 'notices') {
    return page
  }
  throw new Error('Missing Ludork page identity')
}

export function getSiteRoot(): URL {
  return new URL(document.documentElement.dataset.siteRoot!, window.location.href)
}

export function getSiteAssetUrl(path: string): string {
  return new URL(path, getSiteRoot()).href
}

export function getSitePageHref(page: SitePage, language: LanguageKey): string {
  const url = new URL(page === 'home' ? './' : `${page}/`, getSiteRoot())
  url.searchParams.set('lang', language)
  return `${url.pathname}${url.search}`
}
