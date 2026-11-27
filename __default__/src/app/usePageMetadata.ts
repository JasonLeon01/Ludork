import { useEffect } from 'react'
import type { SitePage } from '../data/site'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'

export default function usePageMetadata(page: SitePage, language: LanguageKey) {
  useEffect(() => {
    const messages = LUDORK_SITE_MESSAGES[language]
    document.documentElement.lang = language === 'zh_CN' ? 'zh-CN' : 'en-GB'
    document.title = page === 'home' ? `Ludork — ${messages.footer.description}`
      : messages[page].title
    document.querySelector('meta[name="description"]')?.setAttribute(
      'content', messages[page].description,
    )
  }, [page, language])
}
