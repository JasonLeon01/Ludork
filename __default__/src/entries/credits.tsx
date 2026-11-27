import SiteShell from '../app/SiteShell'
import CreditsPage from '../pages/CreditsPage'
import { mountPage } from './mountPage'

mountPage(<SiteShell page="credits" renderPage={(language) => <CreditsPage language={language} />} />)
