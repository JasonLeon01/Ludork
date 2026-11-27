import { createContext, useContext } from 'react'

export type ThemePreference = 'system' | 'light' | 'dark'
export type ResolvedTheme = 'light' | 'dark'

export type ThemeContextValue = {
  preference: ThemePreference
  resolvedTheme: ResolvedTheme
  systemTheme: ResolvedTheme
  setPreference: (preference: ThemePreference) => void
}

export const THEME_STORAGE_KEY = 'ludork:theme'

export function isThemePreference(value: string | null): value is ThemePreference {
  return value === 'system' || value === 'light' || value === 'dark'
}

export function readThemePreference(): ThemePreference {
  try {
    const value = window.localStorage.getItem(THEME_STORAGE_KEY)
    return value === 'light' || value === 'dark' ? value : 'system'
  } catch {
    return 'system'
  }
}

export function saveThemePreference(preference: ThemePreference): void {
  try {
    if (preference === 'system') {
      window.localStorage.removeItem(THEME_STORAGE_KEY)
    } else {
      window.localStorage.setItem(THEME_STORAGE_KEY, preference)
    }
  } catch {
    // Keep theme switching usable when storage is restricted.
  }
}

export function applyThemePreference(preference: ThemePreference): void {
  if (preference === 'system') {
    delete document.documentElement.dataset.theme
  } else {
    document.documentElement.dataset.theme = preference
  }
}

export function getNextThemePreference(preference: ThemePreference, systemTheme: ResolvedTheme): ThemePreference {
  const opposite: ResolvedTheme = systemTheme === 'dark' ? 'light' : 'dark'
  if (preference === 'system') return opposite
  if (preference === opposite) return systemTheme
  return 'system'
}

export const ThemeContext = createContext<ThemeContextValue | null>(null)

export function useThemePreference(): ThemeContextValue {
  const context = useContext(ThemeContext)
  if (!context) {
    throw new Error('useThemePreference must be used within Theme')
  }
  return context
}
