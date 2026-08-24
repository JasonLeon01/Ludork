import { useCallback, useEffect, useMemo, useState } from 'react'
import {
  Box,
  CssBaseline,
  Drawer,
  FormControl,
  IconButton,
  MenuItem,
  Select,
  Typography,
  useMediaQuery,
  useTheme,
} from '@mui/material'
import type { SelectChangeEvent } from '@mui/material'
import LudorkSidebar from './LudorkSidebar'
import LudorkContent from './LudorkContent'
import LudorkSectionTabs from './LudorkSectionTabs'
import { GitHubIcon, HomeIcon, MenuIcon } from './LudorkIcon'
import {
  docKeyFromFilename,
  findSectionByFilename,
  flattenDocFilenames,
  getDocsHomePath,
  getDocsSections,
  resolveKnownDocPath,
  resolveFilenameByDocKey,
  type DocSection,
  type SelectedDoc,
} from './ludorkDocsIndex'
import { LUDORK_LANGUAGES, LUDORK_LANGUAGE_KEYS, type LanguageKey } from './ludorkLanguages'
import {
  detectLudorkLanguageFromBrowser,
  parseLudorkDoc,
  parseLudorkLanguage,
  parseLudorkPath,
  resolveInitialLudorkLanguage,
  setLudorkDocInUrl,
  setLudorkLanguageInUrl,
  setLudorkPathInUrl,
} from './ludorkUrl'
import './ludork.css'

const SIDEBAR_WIDTH = 280

type SectionLocation = {
  section: DocSection
  selection: SelectedDoc
}

type RememberedSelections = Partial<Record<string, SelectedDoc>>

function selectedFromDocKey(lang: LanguageKey, docKey: string): SelectedDoc {
  return { type: 'doc', lang, docKey }
}

function filenamesFromSections(sections: readonly DocSection[]): string[] {
  return sections.flatMap((section) => flattenDocFilenames(section.items))
}

function filenameFromDocKey(lang: LanguageKey, docKey: string): string | null {
  return resolveFilenameByDocKey(filenamesFromSections(getDocsSections(lang)), docKey)
}

function locationFromSelection(
  language: LanguageKey,
  sections: readonly DocSection[],
  selection: SelectedDoc,
): SectionLocation | null {
  if (selection.type === 'home') {
    const section = sections.find((candidate) => candidate.includesHome)
    return section ? { section, selection } : null
  }

  const filename = resolveFilenameByDocKey(filenamesFromSections(sections), selection.docKey)
  if (!filename) {
    return null
  }

  const section = findSectionByFilename(sections, filename)
  return section
    ? { section, selection: selectedFromDocKey(language, selection.docKey) }
    : null
}

function relativeFilenameFromPath(path: string): string {
  const normalized = path.replace(/\\/g, '/').replace(/^\/+/, '')
  const pathLanguage = LUDORK_LANGUAGE_KEYS.find((key) => normalized.startsWith(`${key}/`))
  return pathLanguage ? normalized.slice(pathLanguage.length + 1) : normalized
}

function locationFromPath(
  language: LanguageKey,
  sections: readonly DocSection[],
  path: string,
): SectionLocation | null {
  const normalized = path.replace(/\\/g, '/').replace(/^\/+/, '')
  if (LUDORK_LANGUAGE_KEYS.some((key) => normalized === getDocsHomePath(key))) {
    return locationFromSelection(language, sections, { type: 'home' })
  }

  const docKey = docKeyFromFilename(relativeFilenameFromPath(normalized))
  return locationFromSelection(language, sections, selectedFromDocKey(language, docKey))
}

function firstSelection(language: LanguageKey, section: DocSection): SelectedDoc | null {
  if (section.includesHome) {
    return { type: 'home' }
  }

  const filename = flattenDocFilenames(section.items)[0]
  return filename ? selectedFromDocKey(language, docKeyFromFilename(filename)) : null
}

function rememberedSelectionKey(language: LanguageKey, sectionKey: string): string {
  return `${language}:${sectionKey}`
}

function sameSelection(left: SelectedDoc | undefined, right: SelectedDoc): boolean {
  if (!left || left.type !== right.type) {
    return false
  }
  if (left.type === 'home') {
    return true
  }
  return right.type === 'doc' && left.docKey === right.docKey
}

function initialSelected(): SelectedDoc {
  const lang = resolveInitialLudorkLanguage()
  const docKey = parseLudorkDoc()
  return docKey && filenameFromDocKey(lang, docKey)
    ? selectedFromDocKey(lang, docKey)
    : { type: 'home' }
}

function initialFreePath(): string | null {
  const language = resolveInitialLudorkLanguage()
  const docKey = parseLudorkDoc()
  return docKey && filenameFromDocKey(language, docKey) ? null : parseLudorkPath()
}

export default function LudorkApp() {
  const [language, setLanguage] = useState<LanguageKey>(() => resolveInitialLudorkLanguage())
  const [selected, setSelected] = useState<SelectedDoc>(initialSelected)
  const [freePath, setFreePath] = useState<string | null>(initialFreePath)
  const [contentHash, setContentHash] = useState(() => window.location.hash)
  const [rememberedSelections, setRememberedSelections] = useState<RememberedSelections>({})
  const theme = useTheme()
  const isNarrow = useMediaQuery(theme.breakpoints.down('md'))
  const [desktopCollapsed, setDesktopCollapsed] = useState(false)
  const [mobileOpen, setMobileOpen] = useState(false)
  const collapsed = isNarrow ? !mobileOpen : desktopCollapsed
  const sections = useMemo(() => getDocsSections(language), [language])

  useEffect(() => {
    document.title = 'Ludork'
    const fromUrl = parseLudorkLanguage()
    if (!fromUrl) {
      setLudorkLanguageInUrl(language, true)
    }

    const docKey = parseLudorkDoc()
    if (!freePath && docKey && !filenameFromDocKey(language, docKey)) {
      setLudorkDocInUrl(language, null, '', true)
    }
  }, [freePath, language])

  useEffect(() => {
    const onPopState = () => {
      const lang = parseLudorkLanguage() ?? detectLudorkLanguageFromBrowser()
      setLanguage(lang)
      setContentHash(window.location.hash)

      const pathParam = parseLudorkPath()
      const docKey = parseLudorkDoc()
      if (docKey && filenameFromDocKey(lang, docKey)) {
        setFreePath(null)
        setSelected(selectedFromDocKey(lang, docKey))
      } else if (pathParam) {
        setFreePath(pathParam)
        const pathLocation = locationFromPath(lang, getDocsSections(lang), pathParam)
        setSelected(pathLocation?.selection ?? { type: 'home' })
      } else {
        setFreePath(null)
        setSelected({ type: 'home' })
        if (docKey) {
          setLudorkDocInUrl(lang, null, window.location.hash, true)
        }
      }
    }

    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [])

  const selectedFilename = useMemo<string | null>(() => {
    if (selected.type !== 'doc') {
      return null
    }
    return filenameFromDocKey(language, selected.docKey)
  }, [language, selected])

  const selectedLocation = useMemo(
    () => locationFromSelection(language, sections, selected),
    [language, sections, selected],
  )
  const pathLocation = useMemo(
    () => freePath ? locationFromPath(language, sections, freePath) : null,
    [freePath, language, sections],
  )
  const currentLocation = pathLocation ?? selectedLocation
  const activeSection = currentLocation?.section ?? sections[0]
  const visibleSelection = currentLocation?.selection ?? selected
  const activeSectionDocumentCount = activeSection
    ? flattenDocFilenames(activeSection.items).length + (activeSection.includesHome ? 1 : 0)
    : 0
  const showSidebar = activeSectionDocumentCount > 1

  const handleToggle = useCallback(() => {
    if (isNarrow) {
      setMobileOpen((previous) => !previous)
    } else {
      setDesktopCollapsed((previous) => !previous)
    }
  }, [isNarrow])

  const rememberSelection = useCallback(
    (targetLanguage: LanguageKey, sectionKey: string, selection: SelectedDoc) => {
      const key = rememberedSelectionKey(targetLanguage, sectionKey)
      setRememberedSelections((previous) => {
        if (sameSelection(previous[key], selection)) {
          return previous
        }
        return { ...previous, [key]: selection }
      })
    },
    [],
  )

  const handleSelect = useCallback(
    (doc: SelectedDoc) => {
      const location = locationFromSelection(language, sections, doc)
      if (location) {
        rememberSelection(language, location.section.key, location.selection)
      }
      setFreePath(null)
      setSelected(doc)
      setContentHash('')
      setLudorkDocInUrl(language, doc.type === 'home' ? null : doc.docKey)
      if (isNarrow) {
        setMobileOpen(false)
      }
    },
    [isNarrow, language, rememberSelection, sections],
  )

  const handleNavigate = useCallback((targetPath: string, hash: string) => {
    const knownDocument = resolveKnownDocPath(targetPath)
    if (knownDocument) {
      const targetSections = getDocsSections(knownDocument.language)
      const targetSelection = knownDocument.docKey
        ? selectedFromDocKey(knownDocument.language, knownDocument.docKey)
        : { type: 'home' } as const
      const targetLocation = locationFromSelection(
        knownDocument.language,
        targetSections,
        targetSelection,
      )
      if (targetLocation) {
        rememberSelection(
          knownDocument.language,
          targetLocation.section.key,
          targetLocation.selection,
        )
      }
      setLanguage(knownDocument.language)
      setFreePath(null)
      setSelected(targetSelection)
      setContentHash(hash)
      setLudorkDocInUrl(knownDocument.language, knownDocument.docKey, hash)
      if (isNarrow) {
        setMobileOpen(false)
      }
      return
    }

    const location = locationFromPath(language, sections, targetPath)
    if (location) {
      rememberSelection(language, location.section.key, location.selection)
    }
    setFreePath(targetPath)
    setContentHash(hash)
    setLudorkPathInUrl(targetPath, hash)
  }, [isNarrow, language, rememberSelection, sections])

  const handleSectionSelect = useCallback((sectionKey: string) => {
    const targetSection = sections.find((section) => section.key === sectionKey)
    if (!targetSection) {
      return
    }

    if (currentLocation) {
      rememberSelection(language, currentLocation.section.key, currentLocation.selection)
    }

    const remembered = rememberedSelections[rememberedSelectionKey(language, sectionKey)]
    const rememberedLocation = remembered
      ? locationFromSelection(language, sections, remembered)
      : null
    const nextSelection = rememberedLocation?.section.key === sectionKey
      ? rememberedLocation.selection
      : firstSelection(language, targetSection)
    if (nextSelection) {
      handleSelect(nextSelection)
    }
  }, [currentLocation, handleSelect, language, rememberSelection, rememberedSelections, sections])

  const contentPath = useMemo<string | null>(() => {
    if (freePath) {
      return freePath
    }
    if (selected.type === 'home') {
      return getDocsHomePath(language)
    }
    return selectedFilename
      ? `${language}/${selectedFilename}`
      : getDocsHomePath(language)
  }, [freePath, language, selected.type, selectedFilename])

  const drawerPaperSx = {
    width: SIDEBAR_WIDTH,
    boxSizing: 'border-box',
  }

  return (
    <>
      <CssBaseline />
      <Box sx={{ display: 'flex', flexDirection: 'column', height: '100vh', overflow: 'hidden' }}>
        <Box
          sx={{
            display: 'flex',
            alignItems: 'center',
            gap: 0.5,
            px: { xs: 1.5, sm: 3 },
            py: 1,
            borderBottom: 1,
            borderColor: 'divider',
            bgcolor: 'background.paper',
          }}
        >
          <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mr: 'auto', minWidth: 0 }}>
            <Box
              component="img"
              src={`${import.meta.env.BASE_URL}favicon.svg`}
              alt=""
              aria-hidden="true"
              sx={{ display: 'block', width: 36, height: 36, flexShrink: 0 }}
            />
            <Typography component="div" variant="h6" noWrap sx={{ fontWeight: 700 }}>
              Ludork
            </Typography>
          </Box>
          <IconButton
            component="a"
            href="https://github.com/JasonLeon01/Ludork"
            target="_blank"
            rel="noreferrer"
            size="small"
            aria-label="GitHub"
          >
            <GitHubIcon />
          </IconButton>
          <IconButton
            component="a"
            href="https://jasonleon01.github.io/"
            target="_blank"
            rel="noreferrer"
            size="small"
            aria-label="Homepage"
          >
            <HomeIcon />
          </IconButton>
          <FormControl size="small" sx={{ minWidth: 120 }}>
            <Select
              value={language}
              onChange={(event: SelectChangeEvent) => {
                const next = event.target.value as LanguageKey
                if (currentLocation) {
                  rememberSelection(language, currentLocation.section.key, currentLocation.selection)
                }
                const currentSelection = currentLocation?.selection ?? selected
                const nextSelection = currentSelection.type === 'doc'
                  && filenameFromDocKey(next, currentSelection.docKey)
                  ? selectedFromDocKey(next, currentSelection.docKey)
                  : { type: 'home' } as const
                setLanguage(next)
                setFreePath(null)
                setSelected(nextSelection)
                setContentHash('')
                setLudorkDocInUrl(
                  next,
                  nextSelection.type === 'doc' ? nextSelection.docKey : null,
                )
              }}
            >
              {LUDORK_LANGUAGE_KEYS.map((langKey) => (
                <MenuItem key={langKey} value={langKey}>
                  {LUDORK_LANGUAGES[langKey].label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>

        <LudorkSectionTabs
          sections={sections}
          activeSectionKey={activeSection?.key ?? false}
          onSelect={handleSectionSelect}
        />

        <Box sx={{ display: 'flex', flex: 1, minHeight: 0, overflow: 'hidden', position: 'relative' }}>
          {showSidebar && activeSection && (
            isNarrow ? (
              !collapsed && (
                <>
                  <Box
                    onClick={() => setMobileOpen(false)}
                    sx={{ position: 'absolute', inset: 0, zIndex: 1199, bgcolor: 'rgba(0,0,0,0.5)' }}
                  />
                  <Box
                    sx={{
                      position: 'absolute',
                      top: 0,
                      left: 0,
                      bottom: 0,
                      width: SIDEBAR_WIDTH,
                      zIndex: 1200,
                      display: 'flex',
                    }}
                  >
                    <LudorkSidebar
                      language={language}
                      section={activeSection}
                      selected={visibleSelection}
                      onSelect={handleSelect}
                    />
                  </Box>
                </>
              )
            ) : (
              !collapsed && (
                <Drawer
                  variant="permanent"
                  sx={{
                    width: SIDEBAR_WIDTH,
                    height: '100%',
                    flexShrink: 0,
                    '& .MuiDrawer-paper': {
                      ...drawerPaperSx,
                      position: 'relative',
                      height: '100%',
                    },
                  }}
                >
                  <LudorkSidebar
                    language={language}
                    section={activeSection}
                    selected={visibleSelection}
                    onSelect={handleSelect}
                    onToggle={handleToggle}
                  />
                </Drawer>
              )
            )
          )}

          {showSidebar && collapsed && (
            <IconButton
              onClick={handleToggle}
              sx={{
                position: 'absolute',
                top: 8,
                left: 8,
                zIndex: 1200,
                bgcolor: 'background.paper',
                boxShadow: 2,
                '&:hover': { bgcolor: 'action.hover' },
              }}
              aria-label="Expand sidebar"
            >
              <MenuIcon />
            </IconButton>
          )}

          <Box component="main" sx={{ flex: 1, minWidth: 0, height: '100%', overflow: 'auto' }}>
            <LudorkContent path={contentPath} hash={contentHash} onNavigate={handleNavigate} />
          </Box>
        </Box>
      </Box>
    </>
  )
}
