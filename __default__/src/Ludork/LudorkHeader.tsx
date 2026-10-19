import LudorkLanguageSelect from './LudorkLanguageSelect'
import { type LanguageKey } from './ludorkLanguages'
import { getSiteAssetUrl, getSitePageHref, LUDORK_LINKS, type SitePage } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { ChevronLeftIcon, GitHubIcon, MenuIcon } from './LudorkIcon'
import './ludorkSite.css'

type LudorkHeaderProps = {
  page: SitePage
  language: LanguageKey
  onLanguageChange?: (language: LanguageKey) => void
  onToggleNavigation?: () => void
  navigationExpanded?: boolean
}

export default function LudorkHeader({
  page,
  language,
  onLanguageChange,
  onToggleNavigation,
  navigationExpanded = false,
}: LudorkHeaderProps) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <header className="ludork-header">
      <a className="ludork-skip-link" href="#main-content">{messages.skipToContent}</a>
      <div className="ludork-header-inner">
        <div className="ludork-header-brand">
          {onToggleNavigation && (
            <button
              className="ludork-navigation-toggle"
              type="button"
              onClick={onToggleNavigation}
              aria-label={navigationExpanded ? messages.docs.collapseSidebar : messages.docs.expandSidebar}
              aria-expanded={navigationExpanded}
              aria-controls="document-navigation"
            >
              {navigationExpanded ? <ChevronLeftIcon /> : <MenuIcon />}
            </button>
          )}
          <a className="ludork-brand" href={getSitePageHref('home', language)} aria-label="Ludork">
            <img src={getSiteAssetUrl('favicon.svg')} width="36" height="36" alt="" />
            <span className="ludork-brand-copy">
              <span>Ludork</span>
              <span className="ludork-brand-description" lang="en">{messages.footer.description}</span>
            </span>
          </a>
        </div>
        <nav className="ludork-navigation" aria-label={messages.navigation.label}>
          <div className="ludork-navigation-primary">
            {(['home', 'docs'] as const).map((item) => (
              <a
                key={item}
                href={getSitePageHref(item, language)}
                aria-current={item === page ? 'page' : undefined}
              >
                {messages.navigation[item]}
              </a>
            ))}
          </div>
          <div className="ludork-navigation-settings">
            <a
              href={getSitePageHref('about', language)}
              aria-current={page === 'about' ? 'page' : undefined}
            >
              {messages.navigation.about}
            </a>
            {onLanguageChange && <LudorkLanguageSelect language={language} onChange={onLanguageChange} />}
          </div>
          <a href={LUDORK_LINKS.repository} aria-label={messages.navigation.repository}>
            <GitHubIcon size={18} />GitHub
          </a>
        </nav>
      </div>
    </header>
  )
}
