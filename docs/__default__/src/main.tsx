import { lazy, StrictMode, Suspense } from 'react'
import { createRoot } from 'react-dom/client'
import LudorkSiteApp from './Ludork/LudorkSiteApp'
import LudorkTheme from './Ludork/LudorkTheme'
import { getSitePage } from './Ludork/ludorkSite'
import { LUDORK_SITE_MESSAGES } from './Ludork/ludorkSiteMessages'
import { resolveInitialLudorkLanguage } from './Ludork/ludorkUrl'
import './index.css'

const LudorkApp = lazy(() => import('./Ludork/LudorkApp'))

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <LudorkTheme>
      <Suspense fallback={<p role="status">{LUDORK_SITE_MESSAGES[resolveInitialLudorkLanguage()].docs.loading}</p>}>
        {getSitePage() === 'docs' ? <LudorkApp /> : <LudorkSiteApp />}
      </Suspense>
    </LudorkTheme>
  </StrictMode>,
)
