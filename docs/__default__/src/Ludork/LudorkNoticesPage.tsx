import LudorkContent from './LudorkContent'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_LINKS } from './ludorkSite'

const NOTICE_FILES: Record<LanguageKey, string> = {
  en_GB: 'THIRD_PARTY_NOTICES.md',
  zh_CN: 'THIRD_PARTY_NOTICES_zh_CN.md',
}

const REPOSITORY_LINKS = {
  '../LICENSE.md': LUDORK_LINKS.license,
  '../Licenses/README.md': `${LUDORK_LINKS.repository}/blob/main/Licenses/README.md`,
  '../Licenses/README_zh_CN.md': `${LUDORK_LINKS.repository}/blob/main/Licenses/README_zh_CN.md`,
}

export default function LudorkNoticesPage({ language }: { language: LanguageKey }) {
  return (
    <main id="main-content" tabIndex={-1} className="ludork-notices ludork-container">
      <LudorkContent
        path={NOTICE_FILES[language]}
        language={language}
        hash={window.location.hash}
        linkTargets={REPOSITORY_LINKS}
      />
    </main>
  )
}
