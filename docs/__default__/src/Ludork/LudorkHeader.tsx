import { FormControl, MenuItem, Select } from '@mui/material'
import { LUDORK_LANGUAGES, LUDORK_LANGUAGE_KEYS, type LanguageKey } from './ludorkLanguages'
import { getSiteAssetUrl, getSitePageHref, LUDORK_LINKS, type SitePage } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { GitHubIcon } from './LudorkIcon'
import './ludorkSite.css'

type LudorkHeaderProps = {
  page: SitePage
  language: LanguageKey
  onLanguageChange: (language: LanguageKey) => void
}

export default function LudorkHeader({ page, language, onLanguageChange }: LudorkHeaderProps) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <header className="ludork-header">
      <a className="ludork-skip-link" href="#main-content">{messages.skipToContent}</a>
      <div className="ludork-header-inner">
        <a className="ludork-brand" href={getSitePageHref('home', language)} aria-label="Ludork">
          <img src={getSiteAssetUrl('favicon.svg')} width="36" height="36" alt="" />
          <span className="ludork-brand-copy">
            <span>Ludork</span>
            <span className="ludork-brand-description" lang="en">{messages.footer.description}</span>
          </span>
        </a>
        <nav className="ludork-navigation" aria-label={messages.navigation.label}>
          {(['home', 'docs', 'about'] as const).map((item) => (
            <a
              key={item}
              href={getSitePageHref(item, language)}
              aria-current={item === page ? 'page' : undefined}
            >
              {messages.navigation[item]}
            </a>
          ))}
          <a href={LUDORK_LINKS.repository} aria-label={messages.navigation.repository}>
            <GitHubIcon size={18} />GitHub
          </a>
        </nav>
        <FormControl size="small" className="ludork-language">
          <Select
            value={language}
            sx={{
              borderRadius: '980px',
              '& .MuiSelect-select': { py: 0.85, fontWeight: 500 },
              '&& .MuiSelect-select.MuiInputBase-input': { paddingRight: '2.25rem' },
            }}
            onChange={(event) => onLanguageChange(event.target.value as LanguageKey)}
            inputProps={{ 'aria-label': messages.language }}
          >
            {LUDORK_LANGUAGE_KEYS.map((key) => <MenuItem key={key} value={key}>{LUDORK_LANGUAGES[key].label}</MenuItem>)}
          </Select>
        </FormControl>
      </div>
    </header>
  )
}
