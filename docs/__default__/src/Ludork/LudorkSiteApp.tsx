import { lazy, Suspense, useEffect, useState } from 'react'
import LudorkHeader from './LudorkHeader'
import LudorkHomePage from './LudorkHomePage'
import LudorkFooter from './LudorkFooter'
import type { LanguageKey } from './ludorkLanguages'
import { parseLudorkLanguage, resolveInitialLudorkLanguage } from './ludorkUrl'
import { getSitePage, getSitePageHref } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import useLudorkPageMetadata from './useLudorkPageMetadata'
import './ludorkSite.css'

const LudorkAboutPage = lazy(() => import('./LudorkAboutPage'))

export default function LudorkSiteApp() {
  const page = getSitePage()
  const [language, setLanguage] = useState<LanguageKey>(() => resolveInitialLudorkLanguage())
  useLudorkPageMetadata(page, language)
  useEffect(() => {
    const syncLanguage = () => setLanguage(resolveInitialLudorkLanguage())
    window.addEventListener('popstate', syncLanguage)
    return () => window.removeEventListener('popstate', syncLanguage)
  }, [])
  useEffect(() => {
    if (!parseLudorkLanguage()) {
      history.replaceState(null, '', `${getSitePageHref(page, language)}${window.location.hash}`)
    }
  }, [page, language])
  return (
    <div className="ludork-site">
      <LudorkHeader page={page} language={language} onLanguageChange={(next) => {
        history.pushState(null, '', `${getSitePageHref(page, next)}${window.location.hash}`)
        setLanguage(next)
      }} />
      <Suspense fallback={<p className="ludork-container" role="status">{LUDORK_SITE_MESSAGES[language].docs.loading}</p>}>
        {page === 'about' ? <LudorkAboutPage language={language} /> : <LudorkHomePage language={language} />}
      </Suspense>
      <LudorkFooter language={language} />
    </div>
  )
}
