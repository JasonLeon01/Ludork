import LudorkLanguageSelect from './LudorkLanguageSelect'
import { type LanguageKey } from './ludorkLanguages'
import { getSiteAssetUrl, getSitePageHref, LUDORK_LINKS, type SitePage } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { GitHubIcon } from './LudorkIcon'
import './ludorkSite.css'

type LudorkHeaderProps = {
  page: SitePage
  language: LanguageKey
  onLanguageChange?: (language: LanguageKey) => void
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
        {onLanguageChange && <LudorkLanguageSelect language={language} onChange={onLanguageChange} />}
      </div>
    </header>
  )
}
