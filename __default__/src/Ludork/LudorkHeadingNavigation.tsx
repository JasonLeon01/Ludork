import { useEffect, useId, useRef, useState, type MouseEvent, type RefObject } from 'react'
import { useMediaQuery, useTheme } from '@mui/material'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'

type Heading = { id: string; title: string; level: number }

type LudorkHeadingNavigationProps = {
  contentRef: RefObject<HTMLDivElement | null>
  language: LanguageKey
  getHref: (hash: string) => string
  onNavigate: (event: MouseEvent<HTMLAnchorElement>, hash: string) => void
}

export default function LudorkHeadingNavigation({ contentRef, language, getHref, onNavigate }: LudorkHeadingNavigationProps) {
  const theme = useTheme()
  const isNarrow = useMediaQuery(theme.breakpoints.down('md'))
  const [desktopOpen, setDesktopOpen] = useState(true)
  const [mobileOpen, setMobileOpen] = useState(false)
  const [headings, setHeadings] = useState<Heading[]>([])
  const [activeId, setActiveId] = useState('')
  const buttonRef = useRef<HTMLButtonElement>(null)
  const listId = useId()
  const expanded = isNarrow ? mobileOpen : desktopOpen
  const messages = LUDORK_SITE_MESSAGES[language].docs

  useEffect(() => {
    const content = contentRef.current
    const main = content?.closest('main')
    if (!content || !main) return

    const elements = Array.from(content.querySelectorAll<HTMLHeadingElement>('h2[id], h3[id], h4[id], h5[id], h6[id]'))
    const entries = elements.map((heading) => ({
      id: heading.id,
      title: heading.textContent?.trim() ?? '',
      level: Number(heading.tagName.slice(1)),
    }))
    let frame = 0
    const updateActive = () => {
      frame = 0
      const top = main.getBoundingClientRect().top + 36
      let active = elements[0]?.id ?? ''
      for (const heading of elements) {
        if (heading.getBoundingClientRect().top > top) break
        active = heading.id
      }
      if (main.scrollHeight > main.clientHeight && main.scrollTop + main.clientHeight >= main.scrollHeight - 2) {
        active = elements.at(-1)?.id ?? active
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
    observer.observe(main)
    main.addEventListener('scroll', scheduleUpdate, { passive: true })
    return () => {
      cancelAnimationFrame(frame)
      observer.disconnect()
      main.removeEventListener('scroll', scheduleUpdate)
    }
  }, [contentRef])

  if (!headings.length) return null

  const setExpanded = isNarrow ? setMobileOpen : setDesktopOpen
  const close = () => {
    setExpanded(false)
    buttonRef.current?.focus({ preventScroll: true })
  }

  return (
    <nav
      className="ludork-heading-navigation"
      data-expanded={expanded}
      aria-label={messages.onThisPage}
      onKeyDown={(event) => {
        if (event.key === 'Escape' && expanded) {
          event.preventDefault()
          close()
        }
      }}
    >
      <button
        ref={buttonRef}
        className="ludork-heading-toggle"
        type="button"
        aria-expanded={expanded}
        aria-controls={listId}
        aria-label={expanded ? messages.collapseHeadings : messages.expandHeadings}
        onClick={() => setExpanded((previous) => !previous)}
      >
        <svg viewBox="0 0 10 18" width="10" height="18" fill="none" aria-hidden="true">
          <path d={expanded ? 'M2 1l6 8-6 8' : 'M8 1L2 9l6 8'} stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
      </button>
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
                  if (event.defaultPrevented && isNarrow) close()
                }}
              >
                {heading.title}
              </a>
            </li>
          )
        })}
      </ul>
    </nav>
  )
}
