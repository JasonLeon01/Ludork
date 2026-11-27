import SiteShell from '../app/SiteShell'
import HomePage from '../pages/HomePage'
import { mountPage } from './mountPage'

mountPage(<SiteShell page="home" renderPage={(language) => <HomePage language={language} />} />)
