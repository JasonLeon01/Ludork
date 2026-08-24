import { useEffect, useState, type MouseEvent } from 'react'
import { Box, CircularProgress, Typography } from '@mui/material'
import ReactMarkdown from 'react-markdown'
import rehypeHighlight from 'rehype-highlight'
import rehypeSlug from 'rehype-slug'
import remarkGfm from 'remark-gfm'
import 'highlight.js/styles/github.css'
import { getDocsUrl, resolveDocsReference } from './ludorkDocs'
import { resolveKnownDocPath } from './ludorkDocsIndex'
import { getLudorkDocHref, getLudorkPathHref } from './ludorkUrl'

type LudorkContentProps = {
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

export default function LudorkContent({ path, hash, onNavigate }: LudorkContentProps) {
  const [loadResult, setLoadResult] = useState<MarkdownLoadResult | null>(null)

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
  }, [path])

  const currentResult = loadResult?.path === path ? loadResult : null
  const loading = Boolean(path && !currentResult)
  const error = currentResult?.error ?? null
  const markdown = currentResult?.markdown ?? null

  useEffect(() => {
    if (!markdown || !hash) {
      return
    }
    const frame = requestAnimationFrame(() => scrollToHash(hash))
    return () => cancelAnimationFrame(frame)
  }, [hash, markdown, path])

  if (!path) {
    return (
      <Box sx={{ p: 4, textAlign: 'center', color: 'text.secondary' }}>
        <Typography variant="h6">Select a document from the sidebar</Typography>
      </Box>
    )
  }

  if (loading) {
    return (
      <Box sx={{ display: 'flex', justifyContent: 'center', p: 4 }}>
        <CircularProgress />
      </Box>
    )
  }

  if (error) {
    return (
      <Box sx={{ p: 4, textAlign: 'center', color: 'error.main' }}>
        <Typography variant="h6">Failed to load document</Typography>
        <Typography variant="body2">{error}</Typography>
      </Box>
    )
  }

  return (
    <Box className="ludork-markdown" sx={{ px: { xs: 2, md: 4 }, py: 3, width: '100%', boxSizing: 'border-box' }}>
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
                    if (!shouldHandleNavigation(event)) {
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
