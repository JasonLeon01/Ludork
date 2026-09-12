import type { LanguageKey } from './ludorkLanguages'
import { getSitePageHref, LUDORK_LINKS } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { GitHubIcon } from './LudorkIcon'
import iconCreditsUrl from './assets/credits.md?url&no-inline'

export default function LudorkFooter({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <footer className="ludork-footer">
      <div className="ludork-container ludork-footer-inner">
        <div>
          <a className="ludork-brand" href={getSitePageHref('home', language)}>Ludork</a>
          <p lang="en">{messages.footer.description}</p>
          <p>© 2026 JasonLeon</p>
        </div>
        <div className="ludork-footer-links">
          <a href={LUDORK_LINKS.repository}><GitHubIcon size={18} />{messages.footer.source}</a>
          <a href={LUDORK_LINKS.issues}>{messages.footer.feedback}</a>
          <a href={LUDORK_LINKS.license}>{messages.footer.license}</a>
          <a href={getSitePageHref('notices', language)}>{messages.footer.notices}</a>
          <a href={iconCreditsUrl}>{messages.footer.iconCredits}</a>
        </div>
      </div>
    </footer>
  )
}
