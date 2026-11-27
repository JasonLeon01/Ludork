import { getSitePageHref } from '../data/site'
import {
  DEFAULT_LUDORK_LANGUAGE,
  detectLudorkLanguageFromBrowser,
  isLudorkLanguageKey,
  type LanguageKey,
} from '../i18n/languages'
import { readSavedLanguage } from '../i18n/languageStorage'

const LANG_PARAM = 'lang'
const DOC_PARAM = 'doc'
const PATH_PARAM = 'path'

export function parseLudorkLanguage(search = window.location.search): LanguageKey | null {
  const lang = new URLSearchParams(search).get(LANG_PARAM)
  return lang && isLudorkLanguageKey(lang) ? lang : null
}

export function resolveInitialLudorkLanguage(search = window.location.search): LanguageKey {
  return parseLudorkLanguage(search) ?? readSavedLanguage() ?? detectLudorkLanguageFromBrowser()
}

export function parseLudorkDoc(search = window.location.search): string | null {
  return new URLSearchParams(search).get(DOC_PARAM)
}

export function parseLudorkPath(search = window.location.search): string | null {
  return new URLSearchParams(search).get(PATH_PARAM)
}

function buildDocsUrl(params: URLSearchParams, hash: string): string {
  const query = params.toString()
  const pathname = getSitePageHref('docs', DEFAULT_LUDORK_LANGUAGE).split('?')[0]
  return `${pathname}${query ? `?${query}` : ''}${hash}`
}

function updateHistory(params: URLSearchParams, replace: boolean, hash = window.location.hash): void {
  const url = buildDocsUrl(params, hash)
  if (replace) {
    history.replaceState(null, '', url)
  } else {
    history.pushState(null, '', url)
  }
}

export function setLudorkDocInUrl(
  language: LanguageKey,
  docKey: string | null,
  hash = '',
  replace = false,
): void {
  const params = new URLSearchParams(window.location.search)
  params.set(LANG_PARAM, language)
  params.delete(PATH_PARAM)
  if (docKey) {
    params.set(DOC_PARAM, docKey)
  } else {
    params.delete(DOC_PARAM)
  }
  updateHistory(params, replace, hash)
}

export function setLudorkPathInUrl(path: string, hash = '', replace = false): void {
  const params = new URLSearchParams(window.location.search)
  params.delete(DOC_PARAM)
  params.set(PATH_PARAM, path)
  updateHistory(params, replace, hash)
}

export function getLudorkPathHref(path: string, hash = ''): string {
  const params = new URLSearchParams(window.location.search)
  params.delete(DOC_PARAM)
  params.set(PATH_PARAM, path)
  return buildDocsUrl(params, hash)
}

export function getLudorkDocHref(
  language: LanguageKey,
  docKey: string | null,
  hash = '',
): string {
  const params = new URLSearchParams(window.location.search)
  params.set(LANG_PARAM, language)
  params.delete(PATH_PARAM)
  if (docKey) {
    params.set(DOC_PARAM, docKey)
  } else {
    params.delete(DOC_PARAM)
  }
  return buildDocsUrl(params, hash)
}
