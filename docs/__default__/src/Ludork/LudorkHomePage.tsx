import { Button } from '@mui/material'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_LINKS } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import LudorkPlatformSection from './LudorkPlatformSection'
import { LUDORK_EDITOR_PLATFORMS, LUDORK_GAME_PLATFORMS } from './ludorkPlatforms'
import tilemapWorld from './assets/hero/tilemap-world.webp'

export default function LudorkHomePage({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <main id="main-content" tabIndex={-1} className="ludork-home">
      <section className="ludork-hero ludork-container">
        <div className="ludork-hero-copy">
          <p className="ludork-eyebrow">{messages.home.eyebrow}</p>
          <h1>{messages.home.title}</h1>
          <p className="ludork-lead">{messages.home.description}</p>
          <div className="ludork-actions">
            <Button className="ludork-download" component="a" href={LUDORK_LINKS.releases} variant="contained" size="large" disableElevation>{messages.download}</Button>
          </div>
        </div>
        <div className="ludork-hero-visual">
          <img src={tilemapWorld} alt={messages.home.imageAlt} width="1254" height="1254" fetchPriority="high" decoding="async" />
        </div>
      </section>
      <LudorkPlatformSection id="editor-platforms" title={messages.home.editorPlatforms} platforms={LUDORK_EDITOR_PLATFORMS} />
      <LudorkPlatformSection id="game-platforms" title={messages.home.gamePlatforms} platforms={LUDORK_GAME_PLATFORMS} />
    </main>
  )
}
