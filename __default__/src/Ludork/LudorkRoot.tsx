import { lazy, Suspense } from 'react'
import LudorkSiteApp from './LudorkSiteApp'
import { getSitePage } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { resolveInitialLudorkLanguage } from './ludorkUrl'

const LudorkApp = lazy(() => import('./LudorkApp'))

export default function LudorkRoot() {
  return (
    <Suspense fallback={<div className="ludork-docs-status" role="status"><p>{LUDORK_SITE_MESSAGES[resolveInitialLudorkLanguage()].docs.loading}</p></div>}>
      {getSitePage() === 'docs' ? <LudorkApp /> : <LudorkSiteApp />}
    </Suspense>
  )
}
