import { Button } from '@mui/material'
import Acknowledgements from '../components/Acknowledgements'
import HeroComparison from '../components/HeroComparison'
import PlatformSection from '../components/PlatformSection'
import Reveal from '../components/Reveal'
import { LUDORK_EDITOR_PLATFORMS, LUDORK_GAME_PLATFORMS } from '../data/platforms'
import { LUDORK_LINKS } from '../data/site'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'

export default function HomePage({ language }: { language: LanguageKey }) {
  const messages = LUDORK_SITE_MESSAGES[language]
  return (
    <main id="main-content" tabIndex={-1} className="ludork-home">
      <section className="ludork-hero ludork-container">
        <Reveal className="ludork-hero-copy">
          <p className="ludork-eyebrow">{messages.home.eyebrow}</p>
          <h1>{messages.home.description}</h1>
          <p className="ludork-hero-slogan" lang="en">
            {messages.home.slogan.map((line) => <span key={line}>{line}</span>)}
          </p>
          <div className="ludork-actions">
            <Button className="ludork-download" component="a" href={LUDORK_LINKS.releases} variant="contained" size="large" disableElevation>{messages.download}</Button>
          </div>
        </Reveal>
        <Reveal className="ludork-hero-visual">
          <HeroComparison description={messages.home.imageAlt} editorLabel={messages.home.imageEditorLabel} gameLabel={messages.home.imageGameLabel} />
        </Reveal>
      </section>
      <div className="ludork-platforms">
        <div className="ludork-platform-grid ludork-container ludork-section">
          <PlatformSection id="editor-platforms" title={messages.home.editorPlatforms} platforms={LUDORK_EDITOR_PLATFORMS} />
          <PlatformSection id="game-platforms" title={messages.home.gamePlatforms} platforms={LUDORK_GAME_PLATFORMS} />
        </div>
      </div>
      <Acknowledgements language={language} />
    </main>
  )
}
