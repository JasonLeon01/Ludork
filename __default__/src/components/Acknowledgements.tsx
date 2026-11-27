import { LUDORK_DEPENDENCIES } from '../data/dependencies'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import LogoScroller from './LogoScroller'
import Reveal from './Reveal'

export default function Acknowledgements({ language }: { language: LanguageKey }) {
  return (
    <section className="ludork-acknowledgements" aria-labelledby="acknowledgements-title">
      <Reveal className="ludork-container ludork-section">
        <h2 id="acknowledgements-title">{LUDORK_SITE_MESSAGES[language].acknowledgements.title}</h2>
        <LogoScroller items={LUDORK_DEPENDENCIES} />
      </Reveal>
    </section>
  )
}
