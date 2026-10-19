import { useCallback, useEffect, useMemo, useState } from 'react'
import { Box, IconButton, useMediaQuery } from '@mui/material'
import { isEmbedded } from './ludorkVersions'
import LudorkSidebar from './LudorkSidebar'
import LudorkContent from './LudorkContent'
import LudorkHeader from './LudorkHeader'
import LudorkLanguageSelect from './LudorkLanguageSelect'
import { MenuIcon } from './LudorkIcon'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import useLudorkPageMetadata from './useLudorkPageMetadata'
import {
  getDocsHomePath, getDocsSections, resolveKnownDocPath, resolveFilenameByDocKey, type SelectedDoc,
} from './ludorkDocsIndex'
import type { LanguageKey } from './ludorkLanguages'
import {
  parseLudorkDoc, parseLudorkLanguage, parseLudorkPath, resolveInitialLudorkLanguage,
  setLudorkDocInUrl, setLudorkLanguageInUrl, setLudorkPathInUrl,
} from './ludorkUrl'
import './ludork.css'
import './ludorkSite.css'

type DocLocation = {
  language: LanguageKey
  selected: SelectedDoc
  freePath: string | null
  hash: string
}

function selectionFromPath(language: LanguageKey, path: string): SelectedDoc {
  const docKey = resolveKnownDocPath(path)?.docKey
  return docKey && resolveFilenameByDocKey(language, docKey)
    ? { type: 'doc', lang: language, docKey }
    : { type: 'home' }
}

function readLocation(): DocLocation {
  const language = resolveInitialLudorkLanguage()
  const docKey = parseLudorkDoc()
  const freePath = parseLudorkPath()
  const hash = window.location.hash
  if (docKey && resolveFilenameByDocKey(language, docKey)) {
    return { language, selected: { type: 'doc', lang: language, docKey }, freePath: null, hash }
  }
  return {
    language,
    selected: freePath ? selectionFromPath(language, freePath) : { type: 'home' },
    freePath, hash,
  }
}

export default function LudorkApp() {
  const [location, setLocation] = useState(readLocation)
  const { language, selected, freePath, hash } = location
  const isNarrow = useMediaQuery('(max-width: 1199.95px)')
  const [mobileOpen, setMobileOpen] = useState(false)
  const sections = useMemo(() => getDocsSections(language), [language])
  const messages = LUDORK_SITE_MESSAGES[language]
  useLudorkPageMetadata('docs', language)

  useEffect(() => {
    if (!parseLudorkLanguage()) setLudorkLanguageInUrl(language, true)
    const docKey = parseLudorkDoc()
    if (!freePath && docKey && !resolveFilenameByDocKey(language, docKey)) {
      setLudorkDocInUrl(language, null, '', true)
    }
  }, [freePath, language, selected])

  useEffect(() => {
    const onPopState = () => setLocation(readLocation())
    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [])

  const handleSelect = useCallback((doc: SelectedDoc) => {
    setLocation({ language, selected: doc, freePath: null, hash: '' })
    setLudorkDocInUrl(language, doc.type === 'home' ? null : doc.docKey)
    setMobileOpen(false)
  }, [language])

  const handleNavigate = useCallback((targetPath: string, targetHash: string) => {
    const known = resolveKnownDocPath(targetPath)
    if (known) {
      setLocation({
        language: known.language,
        selected: known.docKey ? { type: 'doc', lang: known.language, docKey: known.docKey } : { type: 'home' },
        freePath: null, hash: targetHash,
      })
      setLudorkDocInUrl(known.language, known.docKey, targetHash)
    } else {
      setLocation({ language, selected: selectionFromPath(language, targetPath), freePath: targetPath, hash: targetHash })
      setLudorkPathInUrl(targetPath, targetHash)
    }
    setMobileOpen(false)
  }, [language])

  const handleLanguageChange = (next: LanguageKey) => {
    const nextSelection: SelectedDoc = selected.type === 'doc' && resolveFilenameByDocKey(next, selected.docKey)
      ? { type: 'doc', lang: next, docKey: selected.docKey }
      : { type: 'home' }
    setLocation({ language: next, selected: nextSelection, freePath: null, hash: '' })
    setLudorkDocInUrl(next, nextSelection.type === 'doc' ? nextSelection.docKey : null)
  }

  const contentPath = freePath ?? (selected.type === 'doc'
    ? `docs/${language}/${resolveFilenameByDocKey(language, selected.docKey)}`
    : getDocsHomePath(language))
  const toggleNavigation = () => setMobileOpen((previous) => !previous)

  return (
    <Box className="ludork-docs-shell" sx={{ display: 'flex', flexDirection: 'column', height: '100dvh', overflow: 'hidden' }}>
      {isEmbedded() ? (
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: 1, px: 2, py: 0.75, flexShrink: 0, borderBottom: '1px solid var(--ludork-line)' }}>
          {isNarrow && <IconButton onClick={toggleNavigation} aria-label={messages.docs.expandSidebar}
            aria-expanded={mobileOpen} aria-controls="document-navigation"><MenuIcon /></IconButton>}
          <Box sx={{ ml: 'auto' }}><LudorkLanguageSelect language={language} onChange={handleLanguageChange} /></Box>
        </Box>
      ) : (
        <LudorkHeader page="docs" language={language} onLanguageChange={handleLanguageChange}
          onToggleNavigation={isNarrow ? toggleNavigation : undefined} navigationExpanded={mobileOpen} />
      )}
      <Box sx={{ display: 'flex', flex: 1, minHeight: 0, overflow: 'hidden', position: 'relative' }}>
        <LudorkSidebar language={language} sections={sections} selected={selected} onSelect={handleSelect}
          isNarrow={isNarrow} open={mobileOpen} onClose={() => setMobileOpen(false)} />
        <Box component="main" id="main-content" tabIndex={-1} className="ludork-docs-main"
          sx={{ flex: 1, minWidth: 0, height: '100%', overflow: 'auto' }}>
          <LudorkContent language={language} path={contentPath} hash={hash} onNavigate={handleNavigate} />
        </Box>
      </Box>
    </Box>
  )
}
