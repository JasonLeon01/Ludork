import SiteShell from '../app/SiteShell'
import NoticesPage from '../pages/NoticesPage'
import { mountPage } from './mountPage'

mountPage(<SiteShell page="notices" renderPage={(language) => <NoticesPage language={language} />} />)
