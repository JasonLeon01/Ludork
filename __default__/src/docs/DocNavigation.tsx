import type { MouseEvent } from 'react'
import { ChevronLeftIcon, ChevronRightIcon } from '../components/Icons'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import type { AdjacentDocs, SelectedDoc } from './docsIndex'
import { getLudorkDocHref } from './urlState'

function shouldHandleNavigation(event: MouseEvent<HTMLAnchorElement>): boolean {
  return event.button === 0 && !event.metaKey && !event.ctrlKey && !event.shiftKey && !event.altKey
}

type DocBreadcrumbsProps = {
  language: LanguageKey
  trail: readonly string[]
  onSelect: (doc: SelectedDoc) => void
}

export function DocBreadcrumbs({ language, trail, onSelect }: DocBreadcrumbsProps) {
  if (!trail.length) return null
  const messages = LUDORK_SITE_MESSAGES[language].docs
  return (
    <nav className="ludork-breadcrumbs" aria-label={messages.breadcrumbs}>
      <ol>
        <li>
          <a
            href={getLudorkDocHref(language, null)}
            onClick={(event) => {
              if (!shouldHandleNavigation(event)) return
              event.preventDefault()
              onSelect({ type: 'home' })
            }}
          >
            {messages.title}
          </a>
        </li>
        {trail.map((label, index) => (
          <li key={`${index}:${label}`} aria-current={index === trail.length - 1 ? 'page' : undefined}>
            <span className="ludork-breadcrumb-sep" aria-hidden="true">
              <ChevronRightIcon size={14} />
            </span>
            <span>{label}</span>
          </li>
        ))}
      </ol>
    </nav>
  )
}

type DocPaginationProps = {
  language: LanguageKey
  adjacent: AdjacentDocs
  onSelect: (doc: SelectedDoc) => void
}

export function DocPagination({ language, adjacent, onSelect }: DocPaginationProps) {
  if (!adjacent.previous && !adjacent.next) return null
  const messages = LUDORK_SITE_MESSAGES[language].docs

  const renderCard = (direction: 'previous' | 'next') => {
    const target = adjacent[direction]
    if (!target) return <span />
    const href = getLudorkDocHref(language, target.selected.type === 'home' ? null : target.selected.docKey)
    return (
      <a
        className={`ludork-pagination-card ludork-pagination-card--${direction}`}
        href={href}
        onClick={(event) => {
          if (!shouldHandleNavigation(event)) return
          event.preventDefault()
          onSelect(target.selected)
        }}
      >
        <span className="ludork-pagination-direction">
          {direction === 'previous' && <ChevronLeftIcon size={15} />}
          <span>{direction === 'previous' ? messages.previousPage : messages.nextPage}</span>
          {direction === 'next' && <ChevronRightIcon size={15} />}
        </span>
        <span className="ludork-pagination-title">{target.title}</span>
      </a>
    )
  }

  return (
    <nav className="ludork-pagination" aria-label={messages.pagination}>
      {renderCard('previous')}
      {renderCard('next')}
    </nav>
  )
}
