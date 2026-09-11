import { useEffect } from 'react'
import type { LanguageKey } from './ludorkLanguages'
import type { SitePage } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'

export default function useLudorkPageMetadata(page: SitePage, language: LanguageKey) {
  useEffect(() => {
    const messages = LUDORK_SITE_MESSAGES[language]
    document.documentElement.lang = language === 'zh_CN' ? 'zh-CN' : 'en-GB'
    document.title = page === 'home' ? `Ludork — ${messages.home.title.replaceAll('\n', ' ')}`
      : page === 'docs' ? messages.docs.title : messages.about.title
    document.querySelector('meta[name="description"]')?.setAttribute(
      'content', page === 'docs' ? messages.docs.description
        : page === 'about' ? messages.about.description : messages.home.description,
    )
  }, [page, language])
}
