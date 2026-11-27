import { useCallback, useEffect, useMemo, useState } from 'react'
import { Box, Button, CircularProgress, IconButton, useMediaQuery } from '@mui/material'
import Header from '../components/Header'
import { MenuIcon, SearchIcon } from '../components/Icons'
import LanguageSelect from '../components/LanguageSelect'
import ThemeToggle from '../components/ThemeToggle'
import { DocBreadcrumbs, DocPagination } from '../docs/DocNavigation'
import DocsSearchDialog from '../docs/DocsSearchDialog'
import {
  getAdjacentDocs, getDocBreadcrumbs, getDocsHomeKey, getDocsHomePath, getDocsSections,
  getLoadedDocsManifest, loadDocsManifest, resolveFilenameByDocKey, resolveKnownDocPath, type SelectedDoc,
} from '../docs/docsIndex'
import type { VersionDocsManifest } from '../docs/docsManifest'
import { getDocsVersion, isEmbedded } from '../docs/docsVersions'
import MarkdownContent from '../docs/MarkdownContent'
import Sidebar from '../docs/Sidebar'
import {
  getLudorkDocHref, getLudorkPathHref, parseLudorkDoc, parseLudorkPath,
  resolveInitialLudorkLanguage, setLudorkDocInUrl, setLudorkPathInUrl,
} from '../docs/urlState'
import type { LanguageKey } from '../i18n/languages'
import { saveLanguage } from '../i18n/languageStorage'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import { MEDIA_QUERIES } from '../styles/breakpoints'
import usePageMetadata from './usePageMetadata'
import '../styles/docs.css'
import '../styles/site.css'

type DocLocation = {
  language: LanguageKey
  selected: SelectedDoc
  freePath: string | null
  hash: string
}

function selectionFromPath(manifest: VersionDocsManifest, language: LanguageKey, path: string): SelectedDoc {
  const docKey = resolveKnownDocPath(manifest, path)?.docKey
  return docKey && resolveFilenameByDocKey(manifest, language, docKey)
    ? { type: 'doc', lang: language, docKey }
    : { type: 'home' }
}

function readLocation(manifest: VersionDocsManifest): DocLocation {
  const language = resolveInitialLudorkLanguage()
  const docKey = parseLudorkDoc()
  const freePath = parseLudorkPath()
  const hash = window.location.hash
  if (docKey && resolveFilenameByDocKey(manifest, language, docKey)) {
    return { language, selected: { type: 'doc', lang: language, docKey }, freePath: null, hash }
  }
  if (freePath) {
    return {
      language,
      selected: selectionFromPath(manifest, language, freePath),
      freePath,
      hash,
    }
  }
  return {
    language,
    selected: { type: 'home' },
    freePath: null,
    hash: docKey ? '' : hash,
  }
}

function DocsView({ manifest, version }: { manifest: VersionDocsManifest; version: string }) {
  const [location, setLocation] = useState<DocLocation>(() => readLocation(manifest))
  const { language, selected, freePath, hash } = location
  const isNarrow = useMediaQuery(MEDIA_QUERIES.narrowNavigation)
  const [mobileOpen, setMobileOpen] = useState(false)
  const [searchOpen, setSearchOpen] = useState(false)
  const sections = useMemo(() => getDocsSections(manifest, language), [manifest, language])
  const messages = LUDORK_SITE_MESSAGES[language]
  const breadcrumbs = useMemo(
    () => (freePath ? [] : getDocBreadcrumbs(manifest, language, selected, messages.docs.faq)),
    [manifest, language, selected, freePath, messages.docs.faq],
  )
  const adjacent = useMemo(
    () => (freePath ? { previous: null, next: null } : getAdjacentDocs(manifest, language, selected, messages.docs.title, messages.docs.faq)),
    [manifest, language, selected, freePath, messages.docs.title, messages.docs.faq],
  )
  const homeKey = useMemo(() => getDocsHomeKey(manifest, language), [manifest, language])
  usePageMetadata('docs', language)

  useEffect(() => {
    const docKey = parseLudorkDoc()
    if (!freePath && docKey && !resolveFilenameByDocKey(manifest, language, docKey)) {
      setLudorkDocInUrl(language, null, '', true)
    }
  }, [manifest, freePath, language, selected])

  useEffect(() => {
    const onPopState = () => setLocation(readLocation(manifest))
    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [manifest])

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if ((event.ctrlKey || event.metaKey) && !event.altKey && event.key.toLowerCase() === 'k') {
        event.preventDefault()
        setMobileOpen(false)
        setSearchOpen((previous) => !previous)
      }
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  }, [])

  const handleSelect = useCallback((doc: SelectedDoc) => {
    setLocation({ language, selected: doc, freePath: null, hash: '' })
    setLudorkDocInUrl(language, doc.type === 'home' ? null : doc.docKey)
    setMobileOpen(false)
  }, [language])

  const handleNavigate = useCallback((targetPath: string, targetHash: string) => {
    const known = resolveKnownDocPath(manifest, targetPath)
    if (known) {
      setLocation({
        language: known.language,
        selected: known.docKey ? { type: 'doc', lang: known.language, docKey: known.docKey } : { type: 'home' },
        freePath: null,
        hash: targetHash,
      })
      setLudorkDocInUrl(known.language, known.docKey, targetHash)
    } else {
      setLocation({
        language,
        selected: selectionFromPath(manifest, language, targetPath),
        freePath: targetPath,
        hash: targetHash,
      })
      setLudorkPathInUrl(targetPath, targetHash)
    }
    setMobileOpen(false)
  }, [manifest, language])

  const resolveDocumentHref = useCallback((targetPath: string, targetHash: string) => {
    const known = resolveKnownDocPath(manifest, targetPath)
    return known
      ? getLudorkDocHref(known.language, known.docKey, targetHash)
      : getLudorkPathHref(targetPath, targetHash)
  }, [manifest])

  const handleLanguageChange = (next: LanguageKey) => {
    saveLanguage(next)
    const nextSelection: SelectedDoc = selected.type === 'doc' && resolveFilenameByDocKey(manifest, next, selected.docKey)
      ? { type: 'doc', lang: next, docKey: selected.docKey }
      : { type: 'home' }
    setLocation({ language: next, selected: nextSelection, freePath: null, hash: '' })
    setLudorkDocInUrl(next, nextSelection.type === 'doc' ? nextSelection.docKey : null)
  }

  const contentPath = freePath ?? (selected.type === 'doc'
    ? `docs/${language}/${resolveFilenameByDocKey(manifest, language, selected.docKey)}`
    : getDocsHomePath(manifest, language))
  const toggleNavigation = () => setMobileOpen((previous) => !previous)

  return (
    <Box className="ludork-docs-shell" sx={{ display: 'flex', flexDirection: 'column', height: '100dvh', overflow: 'hidden' }}>
      {isEmbedded() ? (
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 1, px: 2, py: 0.75, flexShrink: 0, borderBottom: '1px solid var(--ludork-line)' }}>
          {isNarrow && <IconButton onClick={toggleNavigation} aria-label={messages.docs.expandSidebar}
            aria-expanded={mobileOpen} aria-controls="document-navigation"><MenuIcon /></IconButton>}
          <Box sx={{ ml: 'auto', display: 'flex', alignItems: 'center', gap: 1 }}>
            <button
              type="button"
              className="ludork-theme-toggle"
              onClick={() => setSearchOpen(true)}
              aria-label={messages.docs.searchButton}
              title={messages.docs.searchButton}
            >
              <SearchIcon size={18} />
            </button>
            <ThemeToggle language={language} />
            <LanguageSelect language={language} onChange={handleLanguageChange} />
          </Box>
        </Box>
      ) : (
        <Header page="docs" language={language} onLanguageChange={handleLanguageChange}
          onToggleNavigation={isNarrow ? toggleNavigation : undefined} navigationExpanded={mobileOpen} />
      )}
      <Box sx={{ display: 'flex', flex: 1, minHeight: 0, overflow: 'hidden', position: 'relative' }}>
        <Sidebar manifest={manifest} language={language} sections={sections} selected={selected} onSelect={handleSelect}
          onOpenSearch={() => setSearchOpen(true)} isNarrow={isNarrow} open={mobileOpen} onClose={() => setMobileOpen(false)} />
        <Box component="main" id="main-content" tabIndex={-1} className="ludork-docs-main"
          sx={{ flex: 1, minWidth: 0, height: '100%', overflow: 'auto' }}>
          <MarkdownContent
            language={language}
            path={contentPath}
            hash={hash}
            header={<DocBreadcrumbs language={language} trail={breadcrumbs} onSelect={handleSelect} />}
            footer={<DocPagination language={language} adjacent={adjacent} onSelect={handleSelect} />}
            resolveDocumentHref={resolveDocumentHref}
            onNavigate={handleNavigate}
          />
        </Box>
      </Box>
      <DocsSearchDialog
        open={searchOpen}
        version={version}
        language={language}
        homeKey={homeKey}
        onClose={() => setSearchOpen(false)}
        onSelect={handleSelect}
      />
    </Box>
  )
}

export default function DocsApp() {
  const version = getDocsVersion()
  const initialLanguage = useMemo(() => resolveInitialLudorkLanguage(), [])
  const [manifest, setManifest] = useState<VersionDocsManifest | null>(() => getLoadedDocsManifest(version))
  const [error, setError] = useState<string | null>(null)
  const [attempt, setAttempt] = useState(0)

  useEffect(() => {
    if (manifest) return
    let cancelled = false
    loadDocsManifest(version).then(
      (loaded) => {
        if (!cancelled) setManifest(loaded)
      },
      (loadError) => {
        if (!cancelled) setError(loadError instanceof Error ? loadError.message : String(loadError))
      },
    )
    return () => {
      cancelled = true
    }
  }, [version, manifest, attempt])

  if (manifest) {
    return <DocsView manifest={manifest} version={version} />
  }

  const messages = LUDORK_SITE_MESSAGES[initialLanguage].docs
  return (
    <Box className="ludork-docs-shell" sx={{ display: 'flex', flexDirection: 'column', height: '100dvh', overflow: 'hidden' }}>
      {!isEmbedded() && <Header page="docs" language={initialLanguage} />}
      <Box component="main" id="main-content" tabIndex={-1} className="ludork-docs-main" sx={{ flex: 1, minWidth: 0, overflow: 'auto' }}>
        {error ? (
          <div className="ludork-docs-status" role="alert">
            <p>{messages.loadError}</p>
            <Button onClick={() => { setError(null); setAttempt((previous) => previous + 1) }}>{messages.retry}</Button>
          </div>
        ) : (
          <div className="ludork-docs-status" role="status" aria-label={messages.loading}>
            <CircularProgress size={28} aria-label={messages.loading} />
          </div>
        )}
      </Box>
    </Box>
  )
}
