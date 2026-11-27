import { useEffect, useId, useMemo, useRef, useState } from 'react'
import { Button, CircularProgress } from '@mui/material'
import { CloseIcon, SearchIcon } from '../components/Icons'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import type { SelectedDoc } from './docsIndex'
import {
  getLoadedDocsSearchIndex, loadDocsSearchIndex, searchDocsIndex, splitHighlightSegments,
  type DocsSearchIndex, type DocsSearchMatch,
} from './searchIndex'

type DocsSearchDialogProps = {
  open: boolean
  version: string
  language: LanguageKey
  homeKey: string
  onClose: () => void
  onSelect: (doc: SelectedDoc) => void
}

type LoadState = {
  cacheKey: string
  index: DocsSearchIndex | null
  error: string | null
}

function HighlightedText({ text, query }: { text: string; query: string }) {
  return (
    <>
      {splitHighlightSegments(text, query).map((segment, index) => (
        segment.match ? <mark key={index}>{segment.text}</mark> : <span key={index}>{segment.text}</span>
      ))}
    </>
  )
}

export default function DocsSearchDialog({
  open,
  version,
  language,
  homeKey,
  onClose,
  onSelect,
}: DocsSearchDialogProps) {
  const inputRef = useRef<HTMLInputElement>(null)
  const listRef = useRef<HTMLUListElement>(null)
  const listboxId = useId()
  const [query, setQuery] = useState('')
  const [rawActiveIndex, setRawActiveIndex] = useState(0)
  const [attempt, setAttempt] = useState(0)
  const [loadState, setLoadState] = useState<LoadState | null>(null)
  const messages = LUDORK_SITE_MESSAGES[language].docs

  const key = `${version}:${language}`
  const cachedIndex = getLoadedDocsSearchIndex(version, language)

  useEffect(() => {
    if (!open || cachedIndex) return
    let cancelled = false
    loadDocsSearchIndex(version, language).then(
      (index) => {
        if (!cancelled) setLoadState({ cacheKey: key, index, error: null })
      },
      (loadError) => {
        if (!cancelled) {
          setLoadState({
            cacheKey: key,
            index: null,
            error: loadError instanceof Error ? loadError.message : String(loadError),
          })
        }
      },
    )
    return () => {
      cancelled = true
    }
  }, [open, version, language, key, cachedIndex, attempt])

  useEffect(() => {
    if (!open) return
    const previousFocus = document.activeElement instanceof HTMLElement ? document.activeElement : null
    const frame = requestAnimationFrame(() => inputRef.current?.focus())
    return () => {
      cancelAnimationFrame(frame)
      previousFocus?.focus({ preventScroll: true })
    }
  }, [open])

  const activeState = loadState?.cacheKey === key ? loadState : null
  const index = cachedIndex ?? activeState?.index ?? null
  const error = cachedIndex ? null : (activeState?.error ?? null)
  const loading = open && !index && !error

  const results = useMemo(
    () => (index ? searchDocsIndex(index, language, query) : []),
    [index, language, query],
  )
  const activeIndex = results.length ? Math.min(rawActiveIndex, results.length - 1) : 0

  useEffect(() => {
    if (!open || !results.length) return
    const activeItem = listRef.current?.querySelector<HTMLElement>(`[data-result-index="${activeIndex}"]`)
    activeItem?.scrollIntoView({ block: 'nearest' })
  }, [open, activeIndex, results.length])

  if (!open) return null

  const selectMatch = (match: DocsSearchMatch) => {
    const selected: SelectedDoc = match.document.key === homeKey
      ? { type: 'home' }
      : { type: 'doc', lang: language, docKey: match.document.key }
    onSelect(selected)
    onClose()
  }

  return (
    <div
      className="ludork-modal-backdrop"
      role="presentation"
      onClick={(event) => {
        if (event.target === event.currentTarget) onClose()
      }}
    >
      <div
        className="ludork-search-dialog"
        role="dialog"
        aria-modal="true"
        aria-label={messages.searchButton}
        onKeyDown={(event) => {
          if (event.key === 'Escape') {
            event.preventDefault()
            onClose()
          } else if (event.key === 'ArrowDown' && results.length) {
            event.preventDefault()
            setRawActiveIndex((current) => (current + 1) % results.length)
          } else if (event.key === 'ArrowUp' && results.length) {
            event.preventDefault()
            setRawActiveIndex((current) => (current - 1 + results.length) % results.length)
          } else if (event.key === 'Enter' && results[activeIndex]) {
            event.preventDefault()
            selectMatch(results[activeIndex])
          }
        }}
      >
        <div className="ludork-search-input-row">
          <span className="ludork-search-input-icon" aria-hidden="true">
            <SearchIcon size={18} />
          </span>
          <input
            ref={inputRef}
            type="search"
            className="ludork-search-input"
            value={query}
            placeholder={messages.searchPlaceholder}
            aria-label={messages.searchPlaceholder}
            aria-controls={listboxId}
            aria-activedescendant={results[activeIndex] ? `${listboxId}-item-${activeIndex}` : undefined}
            onChange={(event) => {
              setQuery(event.target.value)
              setRawActiveIndex(0)
            }}
          />
          <button
            type="button"
            className="ludork-search-close"
            onClick={onClose}
            aria-label={messages.searchCloseHint}
          >
            <CloseIcon size={18} />
          </button>
        </div>

        <div className="ludork-search-body">
          {loading ? (
            <div className="ludork-search-status" role="status">
              <CircularProgress size={22} aria-label={messages.searchLoading} />
              <p>{messages.searchLoading}</p>
            </div>
          ) : error ? (
            <div className="ludork-search-status" role="alert">
              <p>{messages.searchError}</p>
              <Button size="small" onClick={() => { setLoadState(null); setAttempt((previous) => previous + 1) }}>
                {messages.retry}
              </Button>
            </div>
          ) : !query.trim() ? (
            <div className="ludork-search-status">
              <p>{messages.searchEmpty}</p>
            </div>
          ) : results.length === 0 ? (
            <div className="ludork-search-status" role="status">
              <p>{messages.searchNoResults}</p>
            </div>
          ) : (
            <ul ref={listRef} id={listboxId} className="ludork-search-results" role="listbox">
              {results.map((match, idx) => {
                const isActive = idx === activeIndex
                return (
                  <li key={match.document.key} role="presentation">
                    <button
                      id={`${listboxId}-item-${idx}`}
                      type="button"
                      role="option"
                      aria-selected={isActive}
                      data-result-index={idx}
                      className={`ludork-search-result${isActive ? ' is-active' : ''}`}
                      onMouseEnter={() => setRawActiveIndex(idx)}
                      onClick={() => selectMatch(match)}
                    >
                      {(match.sectionLabel || match.heading) && (
                        <span className="ludork-search-result-meta">
                          {match.sectionLabel}
                          {match.sectionLabel && match.heading ? ' · ' : ''}
                          {match.heading && <HighlightedText text={match.heading} query={query} />}
                        </span>
                      )}
                      <span className="ludork-search-result-title">
                        <HighlightedText text={match.document.title} query={query} />
                      </span>
                      {match.snippet && (
                        <span className="ludork-search-result-snippet">
                          <HighlightedText text={match.snippet} query={query} />
                        </span>
                      )}
                    </button>
                  </li>
                )
              })}
            </ul>
          )}
        </div>

        <div className="ludork-search-footer">
          <div className="ludork-search-hints">
            <span><kbd>↑</kbd><kbd>↓</kbd> {messages.searchNavigateHint}</span>
            <span><kbd>↵</kbd> {messages.searchOpenHint}</span>
            <span><kbd>Esc</kbd> {messages.searchCloseHint}</span>
          </div>
          {query.trim() && results.length > 0 && (
            <span className="ludork-search-count">
              {messages.searchResultsCount.replace('{count}', String(results.length))}
            </span>
          )}
        </div>
      </div>
    </div>
  )
}
