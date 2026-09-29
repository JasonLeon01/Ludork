export type LudorkLanguageConfig = {
  label: string
  localeKeys: readonly string[]
}

export const LUDORK_LANGUAGES = {
  en_GB: {
    label: 'English',
    localeKeys: ['en', 'en_gb'],
  },
  zh_CN: {
    label: '简体中文',
    localeKeys: ['zh', 'zh_cn', 'zh_hans'],
  },
} as const satisfies Record<string, LudorkLanguageConfig>

export type LanguageKey = keyof typeof LUDORK_LANGUAGES

export const DEFAULT_LUDORK_LANGUAGE: LanguageKey = 'en_GB'

export const LUDORK_LANGUAGE_KEYS = Object.keys(LUDORK_LANGUAGES) as LanguageKey[]

export function isLudorkLanguageKey(value: string): value is LanguageKey {
  return Object.hasOwn(LUDORK_LANGUAGES, value)
}

function normalizeLocaleTag(tag: string): string {
  return tag.trim().replace(/-/g, '_').toLowerCase()
}

function localeMatches(locale: string, localeKey: string): boolean {
  const key = normalizeLocaleTag(localeKey)
  return locale === key || locale.startsWith(`${key}_`)
}

export function matchLudorkLanguage(tag: string): LanguageKey | null {
  const locale = normalizeLocaleTag(tag)
  if (!locale) {
    return null
  }
  for (const langKey of LUDORK_LANGUAGE_KEYS) {
    if (LUDORK_LANGUAGES[langKey].localeKeys.some((key) => localeMatches(locale, key))) {
      return langKey
    }
  }
  return null
}

function systemLocale(): string {
  return typeof Intl === 'undefined' ? '' : Intl.DateTimeFormat().resolvedOptions().locale
}

export function detectLudorkLanguageFromBrowser(): LanguageKey {
  const candidates = [
    systemLocale(),
    typeof navigator !== 'undefined' ? navigator.language : '',
  ]
  for (const tag of candidates) {
    const match = matchLudorkLanguage(tag)
    if (match) {
      return match
    }
  }
  return DEFAULT_LUDORK_LANGUAGE
}
