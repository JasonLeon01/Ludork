import { useEffect, useState, type ReactNode } from 'react'
import Footer from '../components/Footer'
import Header from '../components/Header'
import LanguageSelect from '../components/LanguageSelect'
import ThemeToggle from '../components/ThemeToggle'
import { getSitePageHref, type SitePage } from '../data/site'
import { isEmbedded } from '../docs/docsVersions'
import { resolveInitialLudorkLanguage } from '../docs/urlState'
import type { LanguageKey } from '../i18n/languages'
import { saveLanguage } from '../i18n/languageStorage'
import usePageMetadata from './usePageMetadata'
import '../styles/site.css'

type SiteShellProps = {
  page: Exclude<SitePage, 'docs'>
  renderPage: (language: LanguageKey) => ReactNode
}

export default function SiteShell({ page, renderPage }: SiteShellProps) {
  const [language, setLanguage] = useState<LanguageKey>(() => resolveInitialLudorkLanguage())
  usePageMetadata(page, language)

  useEffect(() => {
    const syncLanguage = () => setLanguage(resolveInitialLudorkLanguage())
    window.addEventListener('popstate', syncLanguage)
    return () => window.removeEventListener('popstate', syncLanguage)
  }, [])

  const handleLanguageChange = (next: LanguageKey) => {
    saveLanguage(next)
    const preserveHash = page !== 'notices'
    history.pushState(null, '', `${getSitePageHref(page, next)}${preserveHash ? window.location.hash : ''}`)
    setLanguage(next)
    if (page === 'notices') window.scrollTo({ top: 0 })
  }

  return (
    <div className="ludork-site">
      {isEmbedded() ? (
        <div className="ludork-embedded-controls">
          <ThemeToggle language={language} />
          <LanguageSelect language={language} onChange={handleLanguageChange} />
        </div>
      ) : (
        <Header page={page} language={language} onLanguageChange={handleLanguageChange} />
      )}
      {renderPage(language)}
      {!isEmbedded() && <Footer language={language} />}
    </div>
  )
}
