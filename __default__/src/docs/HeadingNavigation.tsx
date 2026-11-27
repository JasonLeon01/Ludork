import { useEffect, useId, useRef, useState, type CSSProperties, type MouseEvent, type RefObject } from 'react'
import { useMediaQuery } from '@mui/material'
import { ChevronLeftIcon } from '../components/Icons'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import { MEDIA_QUERIES } from '../styles/breakpoints'
import { isEmbedded } from './docsVersions'

type Heading = { id: string; title: string; level: number }

const MOBILE_OPEN_KEY = 'ludork:heading-navigation:mobile-open'

function readOpenPreference(key: string, defaultOpen: boolean): boolean {
  try {
    const value = window.localStorage.getItem(key)
    if (value === 'true') return true
    if (value === 'false') return false
  } catch {
    // Browser storage can be unavailable in restricted browsing contexts.
  }
  return defaultOpen
}

function saveOpenPreference(key: string, open: boolean): void {
  try {
    window.localStorage.setItem(key, String(open))
  } catch {
    // Keep the navigation usable when the browser cannot save preferences.
  }
}

type HeadingNavigationProps = {
  contentRef: RefObject<HTMLDivElement | null>
  language: LanguageKey
  scrollMode?: 'main' | 'window'
  getHref: (hash: string) => string
  onNavigate: (event: MouseEvent<HTMLAnchorElement>, hash: string) => void
}

export default function HeadingNavigation({
  contentRef,
  language,
  scrollMode = 'main',
  getHref,
  onNavigate,
}: HeadingNavigationProps) {
  const isMobilePortrait = useMediaQuery(MEDIA_QUERIES.mobilePortrait)
  const [mobileOpen, setMobileOpen] = useState(() => readOpenPreference(MOBILE_OPEN_KEY, false))
  const [headings, setHeadings] = useState<Heading[]>([])
  const [activeId, setActiveId] = useState('')
  const [viewportHeight, setViewportHeight] = useState(0)
  const buttonRef = useRef<HTMLButtonElement>(null)
  const listId = useId()
  const expanded = !isMobilePortrait || mobileOpen
  const messages = LUDORK_SITE_MESSAGES[language].docs

  useEffect(() => {
    const content = contentRef.current
    const main = content?.closest('main')
    if (!content || (!main && scrollMode === 'main')) return

    const elements = Array.from(content.querySelectorAll<HTMLHeadingElement>('h2[id], h3[id], h4[id], h5[id], h6[id]'))
    const entries = elements.map((heading) => ({
      id: heading.id,
      title: heading.textContent?.trim() ?? '',
      level: Number(heading.tagName.slice(1)),
    }))
    let frame = 0
    const updateActive = () => {
      frame = 0
      const top = scrollMode === 'window'
        ? (isEmbedded() ? 36 : 104)
        : main!.getBoundingClientRect().top + 36
      let active = elements[0]?.id ?? ''
      for (const heading of elements) {
        if (heading.getBoundingClientRect().top > top) break
        active = heading.id
      }
      if (scrollMode === 'window') {
        const docEl = document.documentElement
        if (docEl.scrollHeight > window.innerHeight && window.scrollY + window.innerHeight >= docEl.scrollHeight - 2) {
          active = elements.at(-1)?.id ?? active
        }
        setViewportHeight(window.innerHeight)
      } else if (main) {
        if (main.scrollHeight > main.clientHeight && main.scrollTop + main.clientHeight >= main.scrollHeight - 2) {
          active = elements.at(-1)?.id ?? active
        }
        setViewportHeight(main.clientHeight)
      }
      setActiveId(active)
    }
    const scheduleUpdate = () => {
      if (!frame) frame = requestAnimationFrame(updateActive)
    }
    frame = requestAnimationFrame(() => {
      setHeadings(entries)
      updateActive()
    })
    const observer = new ResizeObserver(scheduleUpdate)
    observer.observe(content)
    if (scrollMode === 'window') {
      window.addEventListener('scroll', scheduleUpdate, { passive: true })
      window.addEventListener('resize', scheduleUpdate, { passive: true })
    } else if (main) {
      observer.observe(main)
      main.addEventListener('scroll', scheduleUpdate, { passive: true })
    }
    return () => {
      cancelAnimationFrame(frame)
      observer.disconnect()
      if (scrollMode === 'window') {
        window.removeEventListener('scroll', scheduleUpdate)
        window.removeEventListener('resize', scheduleUpdate)
      } else if (main) {
        main.removeEventListener('scroll', scheduleUpdate)
      }
    }
  }, [contentRef, scrollMode])

  if (!headings.length) return null

  const setPreferredExpanded = (open: boolean) => {
    setMobileOpen(open)
    saveOpenPreference(MOBILE_OPEN_KEY, open)
  }
  const close = (remember: boolean) => {
    setMobileOpen(false)
    if (remember) saveOpenPreference(MOBILE_OPEN_KEY, false)
    buttonRef.current?.focus({ preventScroll: true })
  }

  return (
    <aside
      className="ludork-heading-aside"
      style={viewportHeight ? { '--ludork-main-height': `${viewportHeight}px` } as CSSProperties : undefined}
    >
      <nav
        className="ludork-heading-navigation"
        data-expanded={expanded}
        aria-label={messages.onThisPage}
        onKeyDown={(event) => {
          if (isMobilePortrait && event.key === 'Escape' && expanded) {
            event.preventDefault()
            close(true)
          }
        }}
      >
        {isMobilePortrait ? (
          <button
            ref={buttonRef}
            className="ludork-heading-toggle"
            type="button"
            aria-expanded={expanded}
            aria-controls={listId}
            aria-label={expanded ? messages.collapseHeadings : messages.expandHeadings}
            onClick={() => setPreferredExpanded(!expanded)}
          >
            <span style={{ display: 'inline-flex', transform: expanded ? 'rotate(180deg)' : undefined }}>
              <ChevronLeftIcon size={18} />
            </span>
          </button>
        ) : <p className="ludork-heading-title">{messages.onThisPage}</p>}
        <ul id={listId} className="ludork-heading-list" inert={!expanded} aria-hidden={!expanded}>
          {headings.map((heading) => {
            const hash = `#${encodeURIComponent(heading.id)}`
            return (
              <li key={heading.id}>
                <a
                  href={getHref(hash)}
                  style={{ paddingInlineStart: `${12 + (heading.level - 2) * 12}px` }}
                  aria-current={heading.id === activeId ? 'location' : undefined}
                  onClick={(event) => {
                    onNavigate(event, hash)
                    if (event.defaultPrevented && isMobilePortrait) close(false)
                  }}
                >
                  {heading.title}
                </a>
              </li>
            )
          })}
        </ul>
      </nav>
    </aside>
  )
}
