import LudorkContent from './LudorkContent'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_LINKS } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'

export default function LudorkAboutPage({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <main id="main-content" tabIndex={-1} className="ludork-about ludork-container">
      <div className="ludork-about-heading">
        <p className="ludork-eyebrow">{messages.about.eyebrow}</p>
        <h1>{messages.about.title}</h1>
      </div>
      <LudorkContent path={`About_${language}.md`} hash="" language={language} />
      <section className="ludork-about-links" aria-labelledby="project-links-title">
        <h2 id="project-links-title">{messages.about.linksTitle}</h2>
        <p>{messages.about.sourceDescription}</p>
        <div className="ludork-project-links">
          <a href={LUDORK_LINKS.repository}>{messages.footer.source}</a>
          <a href={LUDORK_LINKS.issues}>{messages.footer.feedback}</a>
          <a href={LUDORK_LINKS.license}>{messages.footer.license}</a>
          <a href={LUDORK_LINKS.notices[language]}>{messages.footer.notices}</a>
        </div>
      </section>
    </main>
  )
}
