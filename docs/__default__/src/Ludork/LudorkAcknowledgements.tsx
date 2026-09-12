import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_DEPENDENCIES } from './ludorkDependencies'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import LudorkLogoScroller from './LudorkLogoScroller'
import LudorkReveal from './LudorkReveal'

export default function LudorkAcknowledgements({ language }: { language: LanguageKey }) {
  return (
    <section className="ludork-acknowledgements" aria-labelledby="acknowledgements-title">
      <LudorkReveal className="ludork-container ludork-section">
        <h2 id="acknowledgements-title">{LUDORK_SITE_MESSAGES[language].acknowledgements.title}</h2>
        <LudorkLogoScroller items={LUDORK_DEPENDENCIES} />
      </LudorkReveal>
    </section>
  )
}
