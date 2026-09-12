import { Button } from '@mui/material'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_LINKS } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import LudorkPlatformSection from './LudorkPlatformSection'
import LudorkReveal from './LudorkReveal'
import LudorkAcknowledgements from './LudorkAcknowledgements'
import LudorkHeroComparison from './LudorkHeroComparison'
import { LUDORK_EDITOR_PLATFORMS, LUDORK_GAME_PLATFORMS } from './ludorkPlatforms'

export default function LudorkHomePage({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <main id="main-content" tabIndex={-1} className="ludork-home">
      <section className="ludork-hero ludork-container">
        <LudorkReveal className="ludork-hero-copy">
          <p className="ludork-eyebrow">{messages.home.eyebrow}</p>
          <h1>{messages.home.title}</h1>
          <p className="ludork-lead">{messages.home.description}</p>
          <div className="ludork-actions">
            <Button className="ludork-download" component="a" href={LUDORK_LINKS.releases} variant="contained" size="large" disableElevation>{messages.download}</Button>
          </div>
        </LudorkReveal>
        <LudorkReveal className="ludork-hero-visual">
          <LudorkHeroComparison description={messages.home.imageAlt} editorLabel={messages.home.imageEditorLabel} gameLabel={messages.home.imageGameLabel} />
        </LudorkReveal>
      </section>
      <LudorkPlatformSection id="editor-platforms" title={messages.home.editorPlatforms} platforms={LUDORK_EDITOR_PLATFORMS} />
      <LudorkPlatformSection id="game-platforms" title={messages.home.gamePlatforms} platforms={LUDORK_GAME_PLATFORMS} />
      <LudorkAcknowledgements language={language} />
    </main>
  )
}
