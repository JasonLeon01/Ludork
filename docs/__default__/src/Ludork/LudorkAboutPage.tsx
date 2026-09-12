import LudorkContent from './LudorkContent'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'

export default function LudorkAboutPage({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <main id="main-content" tabIndex={-1} className="ludork-about ludork-container">
      <div className="ludork-about-heading">
        <p className="ludork-eyebrow">{messages.about.eyebrow}</p>
        <h1>{messages.about.title}</h1>
      </div>
      <div className="ludork-about-body">
        <LudorkContent path={`About_${language}.md`} hash="" language={language} />
      </div>
    </main>
  )
}
