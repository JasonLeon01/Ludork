import { getNextThemePreference, useThemePreference } from '../app/themePreference'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import { MonitorIcon, MoonIcon, SunIcon } from './Icons'

export default function ThemeToggle({ language }: { language: LanguageKey }) {
  const { preference, systemTheme, setPreference } = useThemePreference()
  const messages = LUDORK_SITE_MESSAGES[language].theme
  const next = getNextThemePreference(preference, systemTheme)
  const label = messages.switchLabel
    .replace('{current}', messages[preference])
    .replace('{next}', messages[next])

  return (
    <button
      type="button"
      className="ludork-theme-toggle"
      onClick={() => setPreference(next)}
      aria-label={label}
      title={label}
    >
      {preference === 'light' ? <SunIcon size={18} />
        : preference === 'dark' ? <MoonIcon size={18} />
          : <MonitorIcon size={18} />}
    </button>
  )
}
