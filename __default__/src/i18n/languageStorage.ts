import { isLudorkLanguageKey, type LanguageKey } from './languages'

const LANGUAGE_STORAGE_KEY = 'ludork:language'

export function readSavedLanguage(): LanguageKey | null {
  try {
    const saved = window.localStorage.getItem(LANGUAGE_STORAGE_KEY)
    return saved && isLudorkLanguageKey(saved) ? saved : null
  } catch {
    return null
  }
}

export function saveLanguage(language: LanguageKey): void {
  try {
    window.localStorage.setItem(LANGUAGE_STORAGE_KEY, language)
  } catch {
    // Keep navigation usable when storage is restricted.
  }
}
