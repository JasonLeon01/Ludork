import MarkdownContent from '../docs/MarkdownContent'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'

export default function AboutPage({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <main id="main-content" tabIndex={-1} className="ludork-about ludork-container">
      <div className="ludork-about-heading">
        <p className="ludork-eyebrow">{messages.about.eyebrow}</p>
        <h1>{messages.about.title}</h1>
      </div>
      <div className="ludork-about-body">
        <MarkdownContent path={`docs/About_${language}.md`} hash="" language={language} />
      </div>
    </main>
  )
}
