import { isValidElement, useEffect, useRef, useState, type MouseEvent, type ReactNode } from 'react'
import { Box, Button, CircularProgress } from '@mui/material'
import ReactMarkdown from 'react-markdown'
import rehypeSlug from 'rehype-slug'
import remarkGfm from 'remark-gfm'
import { CheckIcon, LinkIcon } from '../components/Icons'
import { preserveEmbeddedMode } from '../data/site'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import CodeBlock from './CodeBlock'
import { getDocsUrl, resolveDocsReference } from './docsUrl'
import HeadingNavigation from './HeadingNavigation'
import ImageLightbox from './ImageLightbox'
import rehypeHighlight from './rehypeHighlight'
import { getLudorkPathHref } from './urlState'
import '../styles/docs.css'

type MarkdownContentProps = {
  language: LanguageKey
  path?: string | null
  markdown?: string
  hash?: string
  header?: ReactNode
  footer?: ReactNode
  showHeadingNavigation?: boolean
  scrollMode?: 'main' | 'window'
  resolveDocumentHref?: (path: string, hash: string) => string
  onNavigate?: (targetPath: string, hash: string) => void
}

type MarkdownLoadResult = {
  url: string
  markdown: string | null
  error: string | null
}

type CalloutKind = 'note' | 'tip' | 'important' | 'warning'

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

function extractText(node: ReactNode): string {
  if (typeof node === 'string' || typeof node === 'number') return String(node)
  if (Array.isArray(node)) return node.map(extractText).join('')
  if (isValidElement<{ children?: ReactNode }>(node)) return extractText(node.props.children)
  return ''
}

function detectCalloutKind(children: ReactNode): CalloutKind | undefined {
  const text = extractText(children).trimStart()
  if (/^(?:\[!WARNING\]|\[!CAUTION\]|warning\s*[:：]|caution\s*[:：]|警告\s*[:：]|小心\s*[:：])/i.test(text)) {
    return 'warning'
  }
  if (/^(?:\[!IMPORTANT\]|important\s*[:：]|重要\s*[:：])/i.test(text)) {
    return 'important'
  }
  if (/^(?:\[!TIP\]|tip\s*[:：]|提示\s*[:：])/i.test(text)) {
    return 'tip'
  }
  if (/^(?:\[!NOTE\]|note\s*[:：]|注意\s*[:：])/i.test(text)) {
    return 'note'
  }
  return undefined
}

async function copyHref(href: string): Promise<boolean> {
  try {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(new URL(href, window.location.href).href)
      return true
    }
  } catch {
    // Clipboard access can be restricted in embedded views.
  }
  return false
}

export default function MarkdownContent({
  path = null,
  markdown: staticMarkdown,
  hash = '',
  language,
  header,
  footer,
  showHeadingNavigation,
  scrollMode = 'main',
  resolveDocumentHref,
  onNavigate,
}: MarkdownContentProps) {
  const [loadResult, setLoadResult] = useState<MarkdownLoadResult | null>(null)
  const [attempt, setAttempt] = useState(0)
  const [spinnerUrl, setSpinnerUrl] = useState<string | null>(null)
  const [copiedHeadingId, setCopiedHeadingId] = useState<string | null>(null)
  const [lightboxImage, setLightboxImage] = useState<{ src: string; alt: string } | null>(null)
  const contentRef = useRef<HTMLDivElement>(null)
  const messages = LUDORK_SITE_MESSAGES[language].docs

  const targetUrl = staticMarkdown === undefined && path ? getDocsUrl(path) : null
  const cachedMarkdown = targetUrl ? markdownCache.get(targetUrl) ?? null : null
  const enableHeadingNav = showHeadingNavigation ?? Boolean(onNavigate)

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
    if (!loading || !targetUrl) return
    const timer = window.setTimeout(() => setSpinnerUrl(targetUrl), 150)
    return () => window.clearTimeout(timer)
  }, [loading, targetUrl])

  useEffect(() => {
    if (!copiedHeadingId) return
    const timer = window.setTimeout(() => setCopiedHeadingId(null), 1600)
    return () => window.clearTimeout(timer)
  }, [copiedHeadingId])

  useEffect(() => {
    if (markdown === null) {
      return
    }
    let frame = requestAnimationFrame(() => {
      frame = requestAnimationFrame(() => {
        if (hash) scrollToHash(hash)
        else if (scrollMode === 'main') contentRef.current?.closest('main')?.scrollTo({ top: 0 })
      })
    })
    return () => cancelAnimationFrame(frame)
  }, [hash, markdown, path, scrollMode])

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
        {spinnerUrl === targetUrl && <CircularProgress size={28} aria-label={messages.loading} />}
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

  const getHeadingHref = (targetHash: string): string =>
    onNavigate && path ? getDocumentHref(path, targetHash) : targetHash

  function navigateToHeading(event: MouseEvent<HTMLAnchorElement>, targetHash: string): void {
    if (!shouldHandleNavigation(event)) return
    event.preventDefault()
    if (onNavigate && path) {
      onNavigate(path, targetHash)
      requestAnimationFrame(() => scrollToHash(targetHash))
    } else {
      history.pushState(null, '', `${window.location.pathname}${window.location.search}${targetHash}`)
      scrollToHash(targetHash)
    }
  }

  function navigateToDocument(event: MouseEvent<HTMLAnchorElement>, targetPath: string, targetHash: string): void {
    if (!onNavigate || !shouldHandleNavigation(event)) return
    event.preventDefault()
    onNavigate(targetPath, targetHash)
    if (targetPath === path && targetHash) {
      requestAnimationFrame(() => scrollToHash(targetHash))
    }
  }

  function renderHeading(Tag: 'h2' | 'h3' | 'h4', id: string | undefined, children: ReactNode) {
    if (!id || !enableHeadingNav) {
      return <Tag id={id}>{children}</Tag>
    }
    const targetHash = `#${encodeURIComponent(id)}`
    const href = getHeadingHref(targetHash)
    const isCopied = copiedHeadingId === id
    return (
      <Tag id={id} className="ludork-anchored-heading">
        {children}
        <a
          className={`ludork-heading-anchor${isCopied ? ' is-copied' : ''}`}
          href={href}
          aria-label={isCopied ? messages.copied : messages.copyHeadingLink}
          title={isCopied ? messages.copied : messages.copyHeadingLink}
          onClick={(event) => {
            navigateToHeading(event, targetHash)
            void copyHref(href).then((ok) => {
              if (ok) setCopiedHeadingId(id)
            })
          }}
        >
          {isCopied ? <CheckIcon size={15} /> : <LinkIcon size={15} />}
        </a>
      </Tag>
    )
  }

  const documentKey = path ?? 'static'
  const content = (
    <Box key={documentKey} ref={contentRef} className="ludork-markdown">
      {header}
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[rehypeSlug, rehypeHighlight]}
        components={{
          h2({ id, children }) {
            return renderHeading('h2', id, children)
          },
          h3({ id, children }) {
            return renderHeading('h3', id, children)
          },
          h4({ id, children }) {
            return renderHeading('h4', id, children)
          },
          pre({ children }) {
            return <CodeBlock language={language}>{children}</CodeBlock>
          },
          blockquote({ children }) {
            const callout = detectCalloutKind(children)
            return <blockquote data-callout={callout}>{children}</blockquote>
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
          img({ src, alt, ...rest }) {
            const reference = path && src ? resolveDocsReference(path, src) : null
            const resolvedSrc = reference ? getDocsUrl(reference.path, reference.search, reference.hash) : src
            return (
              <img
                src={resolvedSrc}
                alt={alt ?? ''}
                className="ludork-zoomable-image"
                onClick={(event) => {
                  if (!resolvedSrc || event.currentTarget.closest('a')) return
                  setLightboxImage({ src: resolvedSrc, alt: alt ?? '' })
                }}
                {...rest}
              />
            )
          },
        }}
      >
        {markdown ?? ''}
      </ReactMarkdown>
      {footer}
      {lightboxImage && (
        <ImageLightbox
          src={lightboxImage.src}
          alt={lightboxImage.alt}
          language={language}
          onClose={() => setLightboxImage(null)}
        />
      )}
    </Box>
  )

  return enableHeadingNav ? (
    <div className={`ludork-document-layout${scrollMode === 'window' ? ' ludork-document-layout--window' : ''}`}>
      {content}
      <HeadingNavigation
        key={`headings:${documentKey}`}
        contentRef={contentRef}
        language={language}
        scrollMode={scrollMode}
        getHref={getHeadingHref}
        onNavigate={navigateToHeading}
      />
    </div>
  ) : content
}
