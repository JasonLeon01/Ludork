import creditsMarkdown from '../assets/credits.md?raw'
import MarkdownContent from '../docs/MarkdownContent'
import type { LanguageKey } from '../i18n/languages'

export default function CreditsPage({ language }: { language: LanguageKey }) {
  return (
    <main id="main-content" tabIndex={-1} className="ludork-notices ludork-container">
      <MarkdownContent
        markdown={creditsMarkdown}
        language={language}
        hash={window.location.hash}
        showHeadingNavigation
        scrollMode="window"
      />
    </main>
  )
}
