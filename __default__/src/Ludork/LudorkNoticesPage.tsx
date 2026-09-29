import LudorkContent from './LudorkContent'
import type { LanguageKey } from './ludorkLanguages'

const NOTICE_FILES: Record<LanguageKey, string> = {
  en_GB: 'THIRD_PARTY_NOTICES.md',
  zh_CN: 'THIRD_PARTY_NOTICES_zh_CN.md',
}

export default function LudorkNoticesPage({ language }: { language: LanguageKey }) {
  return (
    <main id="main-content" tabIndex={-1} className="ludork-notices ludork-container">
      <LudorkContent
        path={`docs/${NOTICE_FILES[language]}`}
        language={language}
        hash={window.location.hash}
      />
    </main>
  )
}
