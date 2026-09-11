import { useEffect, useRef, useState, type MouseEvent } from 'react'
import { Box, Button, CircularProgress } from '@mui/material'
import ReactMarkdown from 'react-markdown'
import rehypeHighlight from 'rehype-highlight'
import rehypeSlug from 'rehype-slug'
import remarkGfm from 'remark-gfm'
import { getDocsUrl, resolveDocsReference } from './ludorkDocs'
import { resolveKnownDocPath } from './ludorkDocsIndex'
import { getLudorkDocHref, getLudorkPathHref } from './ludorkUrl'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import './ludork.css'

type LudorkContentProps = {
  language: LanguageKey
  path: string | null
  hash: string
  onNavigate?: (targetPath: string, hash: string) => void
}

type MarkdownLoadResult = {
  path: string
  markdown: string | null
  error: string | null
}

function shouldHandleNavigation(event: MouseEvent<HTMLAnchorElement>): boolean {
  return event.button === 0 && !event.metaKey && !event.ctrlKey && !event.shiftKey && !event.altKey
}

function scrollToHash(hash: string): void {
  if (!hash.startsWith('#')) {
    return
  }

  let id: string
  try {
    id = decodeURIComponent(hash.slice(1))
  } catch {
    return
  }
  document.getElementById(id)?.scrollIntoView({ block: 'start' })
}

export default function LudorkContent({ path, hash, language, onNavigate }: LudorkContentProps) {
  const [loadResult, setLoadResult] = useState<MarkdownLoadResult | null>(null)
  const [attempt, setAttempt] = useState(0)
  const contentRef = useRef<HTMLDivElement>(null)
  const messages = LUDORK_SITE_MESSAGES[language].docs

  useEffect(() => {
    if (!path) {
      return
    }

    const targetPath = path
    let cancelled = false
    const controller = new AbortController()

    async function loadMarkdown(): Promise<void> {
      await Promise.resolve()

      try {
        const response = await fetch(getDocsUrl(targetPath), { signal: controller.signal })
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`)
        }
        if (response.headers.get('content-type')?.includes('text/html')) {
          throw new Error('Unexpected HTML response')
        }
        const text = await response.text()
        if (!cancelled) {
          setLoadResult({ path: targetPath, markdown: text, error: null })
        }
      } catch (loadError) {
        if (!cancelled) {
          setLoadResult({
            path: targetPath,
            markdown: null,
            error: loadError instanceof Error ? loadError.message : String(loadError),
          })
        }
      }
    }

    void loadMarkdown()
    return () => {
      cancelled = true
      controller.abort()
    }
  }, [path, attempt])

  const currentResult = loadResult?.path === path ? loadResult : null
  const loading = Boolean(path && !currentResult)
  const error = currentResult?.error ?? null
  const markdown = currentResult?.markdown ?? null

  useEffect(() => {
    if (!markdown) {
      return
    }
    const frame = requestAnimationFrame(() => {
      if (hash) scrollToHash(hash)
      else contentRef.current?.closest('main')?.scrollTo({ top: 0 })
    })
    return () => cancelAnimationFrame(frame)
  }, [hash, markdown, path])

  if (!path) {
    return (
      <div className="ludork-docs-status">
        <p>{messages.selectDocument}</p>
      </div>
    )
  }

  if (loading) {
    return (
      <div className="ludork-docs-status" role="status" aria-label={messages.loading}>
        <CircularProgress size={28} aria-label={messages.loading} />
      </div>
    )
  }

  if (error) {
    return (
      <div className="ludork-docs-status" role="alert">
        <p>{messages.loadError}</p>
        <Button onClick={() => { setLoadResult(null); setAttempt((previous) => previous + 1) }}>{messages.retry}</Button>
      </div>
    )
  }

  return (
    <Box ref={contentRef} className="ludork-markdown">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[rehypeSlug, rehypeHighlight]}
        components={{
          a({ href, children, ...rest }) {
            const reference = href ? resolveDocsReference(path, href) : null
            if (reference?.path.toLowerCase().endsWith('.md')) {
              const knownDocument = resolveKnownDocPath(reference.path)
              const documentHref = knownDocument
                ? getLudorkDocHref(knownDocument.language, knownDocument.docKey, reference.hash)
                : getLudorkPathHref(reference.path, reference.hash)
              return (
                <a
                  href={documentHref}
                  onClick={(event) => {
                    if (!onNavigate || !shouldHandleNavigation(event)) {
                      return
                    }
                    event.preventDefault()
                    onNavigate?.(reference.path, reference.hash)
                    if (reference.path === path && reference.hash) {
                      requestAnimationFrame(() => scrollToHash(reference.hash))
                    }
                  }}
                  {...rest}
                >
                  {children}
                </a>
              )
            }
            if (reference) {
              return (
                <a href={getDocsUrl(reference.path, reference.search, reference.hash)} {...rest}>
                  {children}
                </a>
              )
            }
            return <a href={href} {...rest}>{children}</a>
          },
          img({ src, ...rest }) {
            const reference = src ? resolveDocsReference(path, src) : null
            return <img src={reference ? getDocsUrl(reference.path, reference.search, reference.hash) : src} {...rest} />
          },
        }}
      >
        {markdown ?? ''}
      </ReactMarkdown>
    </Box>
  )
}
