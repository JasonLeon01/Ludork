import SiteShell from '../app/SiteShell'
import AboutPage from '../pages/AboutPage'
import { mountPage } from './mountPage'

mountPage(<SiteShell page="about" renderPage={(language) => <AboutPage language={language} />} />)
