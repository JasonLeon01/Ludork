import { useEffect, useRef, useState, type MouseEvent } from 'react'
import { Box, Button, CircularProgress } from '@mui/material'
import ReactMarkdown from 'react-markdown'
import rehypeSlug from 'rehype-slug'
import remarkGfm from 'remark-gfm'
import { preserveEmbeddedMode } from '../data/site'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import HeadingNavigation from './HeadingNavigation'
import { getDocsUrl, resolveDocsReference } from './docsUrl'
import rehypeHighlight from './rehypeHighlight'
import { getLudorkPathHref } from './urlState'
import '../styles/docs.css'

type MarkdownContentProps = {
  language: LanguageKey
  path?: string | null
  markdown?: string
  hash?: string
  resolveDocumentHref?: (path: string, hash: string) => string
  onNavigate?: (targetPath: string, hash: string) => void
}

type MarkdownLoadResult = {
  url: string
  markdown: string | null
  error: string | null
}

const markdownCache = new Map<string, string>()

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

export default function MarkdownContent({
  path = null,
  markdown: staticMarkdown,
  hash = '',
  language,
  resolveDocumentHref,
  onNavigate,
}: MarkdownContentProps) {
  const [loadResult, setLoadResult] = useState<MarkdownLoadResult | null>(null)
  const [attempt, setAttempt] = useState(0)
  const contentRef = useRef<HTMLDivElement>(null)
  const messages = LUDORK_SITE_MESSAGES[language].docs

  const targetUrl = staticMarkdown === undefined && path ? getDocsUrl(path) : null
  const cachedMarkdown = targetUrl ? markdownCache.get(targetUrl) ?? null : null

  useEffect(() => {
    if (!targetUrl || markdownCache.has(targetUrl)) {
      return
    }

    const fetchUrl = targetUrl
    let cancelled = false
    const controller = new AbortController()

    async function loadMarkdown(): Promise<void> {
      try {
        const response = await fetch(fetchUrl, { signal: controller.signal })
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`)
        }
        if (response.headers.get('content-type')?.includes('text/html')) {
          throw new Error('Unexpected HTML response')
        }
        const text = await response.text()
        markdownCache.set(fetchUrl, text)
        if (!cancelled) {
          setLoadResult({ url: fetchUrl, markdown: text, error: null })
        }
      } catch (loadError) {
        if (!cancelled) {
          setLoadResult({
            url: fetchUrl,
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
  }, [targetUrl, attempt])

  const currentResult = targetUrl && loadResult?.url === targetUrl ? loadResult : null
  const markdown = staticMarkdown ?? cachedMarkdown ?? currentResult?.markdown ?? null
  const error = staticMarkdown !== undefined || cachedMarkdown !== null ? null : (currentResult?.error ?? null)
  const loading = Boolean(targetUrl && markdown === null && error === null)

  useEffect(() => {
    if (markdown === null) {
      return
    }
    let frame = requestAnimationFrame(() => {
      frame = requestAnimationFrame(() => {
        if (hash) scrollToHash(hash)
        else contentRef.current?.closest('main')?.scrollTo({ top: 0 })
      })
    })
    return () => cancelAnimationFrame(frame)
  }, [hash, markdown, path])

  if (staticMarkdown === undefined && !path) {
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

  const getDocumentHref = (targetPath: string, targetHash: string): string =>
    resolveDocumentHref ? resolveDocumentHref(targetPath, targetHash) : getLudorkPathHref(targetPath, targetHash)

  function navigateToDocument(event: MouseEvent<HTMLAnchorElement>, targetPath: string, targetHash: string): void {
    if (!onNavigate || !shouldHandleNavigation(event)) return
    event.preventDefault()
    onNavigate(targetPath, targetHash)
    if (targetPath === path && targetHash) {
      requestAnimationFrame(() => scrollToHash(targetHash))
    }
  }

  const documentKey = path ?? 'static'
  const content = (
    <Box key={documentKey} ref={contentRef} className="ludork-markdown">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[rehypeSlug, rehypeHighlight]}
        components={{
          pre({ children }) {
            return <pre tabIndex={0}>{children}</pre>
          },
          table({ children }) {
            return (
              <div className="ludork-table-scroll" tabIndex={0}>
                <table>{children}</table>
              </div>
            )
          },
          a({ href, children, ...rest }) {
            const reference = path && href ? resolveDocsReference(path, href) : null
            if (reference?.path.toLowerCase().endsWith('.md')) {
              return (
                <a
                  href={getDocumentHref(reference.path, reference.hash)}
                  onClick={(event) => navigateToDocument(event, reference.path, reference.hash)}
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
            return <a href={preserveEmbeddedMode(href)} {...rest}>{children}</a>
          },
          img({ src, ...rest }) {
            const reference = path && src ? resolveDocsReference(path, src) : null
            return <img src={reference ? getDocsUrl(reference.path, reference.search, reference.hash) : src} {...rest} />
          },
        }}
      >
        {markdown ?? ''}
      </ReactMarkdown>
    </Box>
  )

  return onNavigate && path ? (
    <div className="ludork-document-layout">
      {content}
      <HeadingNavigation
        key={`headings:${path}`}
        contentRef={contentRef}
        language={language}
        getHref={(targetHash) => getDocumentHref(path, targetHash)}
        onNavigate={(event, targetHash) => navigateToDocument(event, path, targetHash)}
      />
    </div>
  ) : content
}
