import { useEffect, useMemo, useRef, useState } from 'react'
import { Box, Divider, Drawer, IconButton, ListItemButton, ListItemText, TextField, Typography, useMediaQuery } from '@mui/material'
import { ChevronLeftIcon } from '../components/Icons'
import VersionSelect from '../components/VersionSelect'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import { MEDIA_QUERIES } from '../styles/breakpoints'
import { getDocsFaqTree, isFaqDoc, type DocSection, type DocTreeItem, type SelectedDoc } from './docsIndex'
import type { VersionDocsManifest } from './docsManifest'
import SidebarItem from './SidebarItem'
import { getLudorkDocHref } from './urlState'

type SidebarProps = {
  manifest: VersionDocsManifest
  language: LanguageKey
  sections: readonly DocSection[]
  selected: SelectedDoc
  onSelect: (doc: SelectedDoc) => void
  isNarrow: boolean
  open: boolean
  onClose: () => void
}

type FilterState = {
  language: LanguageKey
  forSelection: string
  rawQuery: string
  searchExpanded: Record<string, boolean>
}

type ExpansionState = {
  expanded: Record<string, boolean>
  collapsedForSelection: Record<string, string>
}

function filterItems(items: DocTreeItem[], query: string): DocTreeItem[] {
  return items.flatMap((item): DocTreeItem[] => {
    const title = item.type === 'folder' ? item.displayName : item.entry.displayName
    if (title.toLocaleLowerCase().includes(query)) return [item]
    if (item.type === 'doc') return []
    const children = filterItems(item.children, query)
    return children.length ? [{ ...item, children }] : []
  })
}

function selectedAncestors(items: DocTreeItem[], docKey: string): string[] | null {
  for (const item of items) {
    if (item.type === 'doc') {
      if (item.entry.key === docKey) return []
    } else {
      const ancestors = selectedAncestors(item.children, docKey)
      if (ancestors) return [item.key, ...ancestors]
    }
  }
  return null
}

export default function Sidebar({ manifest, language, sections, selected, onSelect, isNarrow, open, onClose }: SidebarProps) {
  const sidebarRef = useRef<HTMLElement>(null)
  const reducedMotion = useMediaQuery(MEDIA_QUERIES.reducedMotion)
  const messages = LUDORK_SITE_MESSAGES[language].docs
  const faqTree = useMemo(() => getDocsFaqTree(manifest, language), [manifest, language])
  const faqSelected = selected.type === 'doc' && isFaqDoc(manifest, language, selected.docKey)
  const tree = useMemo<DocTreeItem[]>(() => sections.map((section) => ({
    type: 'folder', key: section.key, displayName: section.displayName, children: section.items,
  })), [sections])

  const selectionKey = selected.type === 'doc' ? selected.docKey : 'home'
  const selectionScope = `${language}:${selectionKey}`
  const ancestors = useMemo(
    () => (selected.type === 'doc' ? selectedAncestors([faqTree, ...tree], selected.docKey) ?? [] : []),
    [faqTree, tree, selected],
  )
  const ancestorSet = useMemo(() => new Set(ancestors), [ancestors])

  const [expansionState, setExpansionState] = useState<ExpansionState>({
    expanded: {},
    collapsedForSelection: {},
  })
  const [filterState, setFilterState] = useState<FilterState>({
    language,
    forSelection: selectionKey,
    rawQuery: '',
    searchExpanded: {},
  })

  const candidateQuery = filterState.language === language ? filterState.rawQuery : ''
  const normalizedCandidate = candidateQuery.trim().toLocaleLowerCase()
  const filteredCandidateTree = useMemo(
    () => (normalizedCandidate ? filterItems(tree, normalizedCandidate) : tree),
    [tree, normalizedCandidate],
  )
  const keepFilter = Boolean(candidateQuery) && (
    filterState.forSelection === selectionKey
    || selected.type === 'home'
    || faqSelected
    || selectedAncestors(filteredCandidateTree, selectionKey) !== null
  )
  const rawQuery = keepFilter ? candidateQuery : ''
  const query = keepFilter ? normalizedCandidate : ''
  const visibleTree = query ? filteredCandidateTree : tree
  const activeSearchExpanded = filterState.language === language && filterState.forSelection === selectionKey
    ? filterState.searchExpanded
    : {}

  const effectiveExpanded = useMemo(() => {
    const merged: Record<string, boolean> = { ...expansionState.expanded }
    for (const key of ancestors) {
      if (expansionState.collapsedForSelection[key] !== selectionScope) {
        merged[key] = true
      }
    }
    return merged
  }, [expansionState, ancestors, selectionScope])

  const handleToggleFolder = (key: string, nextOpen: boolean) => {
    setExpansionState((previous) => {
      const nextCollapsed = { ...previous.collapsedForSelection }
      if (!nextOpen && ancestorSet.has(key)) {
        nextCollapsed[key] = selectionScope
      } else if (nextOpen) {
        delete nextCollapsed[key]
      }
      return {
        expanded: { ...previous.expanded, [key]: nextOpen },
        collapsedForSelection: nextCollapsed,
      }
    })
  }

  useEffect(() => {
    if (isNarrow && !open) return
    const frame = requestAnimationFrame(() => {
      sidebarRef.current?.querySelector('[aria-current="page"]')?.scrollIntoView({ block: 'nearest' })
    })
    return () => cancelAnimationFrame(frame)
  }, [isNarrow, open, language, selectionKey])

  return (
    <Drawer variant={isNarrow ? 'temporary' : 'permanent'} open={!isNarrow || open}
      onClose={onClose} transitionDuration={reducedMotion ? 0 : 200}
      ModalProps={{ keepMounted: true }} slotProps={{ paper: { id: 'document-navigation' } }}
      sx={{
        width: isNarrow ? 0 : 280, flexShrink: 0,
        '& .MuiDrawer-paper': {
          width: 280, maxWidth: '100vw', boxSizing: 'border-box', overflow: 'hidden',
          borderRight: '1px solid var(--ludork-line)', background: 'var(--ludork-surface)',
          boxShadow: isNarrow ? 'var(--ludork-shadow)' : 'none',
          ...(!isNarrow ? { position: 'relative', height: '100%' } : {}),
        },
      }}>
      <Box ref={sidebarRef} component="nav" className="ludork-sidebar" aria-label={messages.sections}
        sx={{ width: '100%', height: '100%', minHeight: 0, overflowY: 'auto' }}>
        {isNarrow && <Box sx={{ display: 'flex', justifyContent: 'flex-end', px: 1.5, pt: 1 }}>
          <IconButton onClick={onClose} size="small" aria-label={messages.collapseSidebar}><ChevronLeftIcon /></IconButton>
        </Box>}
        <Box sx={{ pt: 1.5, pb: 1 }}>
          <ListItemButton component="a" href={getLudorkDocHref(language, null)}
            selected={selected.type === 'home'} aria-current={selected.type === 'home' ? 'page' : undefined}
            onClick={(event) => {
              if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
              event.preventDefault()
              onSelect({ type: 'home' })
            }}>
            <ListItemText primary={messages.title} slotProps={{ primary: { sx: { fontSize: 14, fontWeight: 600 } } }} />
          </ListItemButton>
          <SidebarItem item={{ ...faqTree, displayName: messages.faq }} language={language} selected={selected}
            onSelect={onSelect} expanded={effectiveExpanded} defaultExpanded={false}
            onToggle={handleToggleFolder} />
        </Box>
        <Divider sx={{ mx: 2 }} />
        <Box sx={{ px: 2, py: 1.5, display: 'grid', gap: 1 }}>
          <VersionSelect language={language} selected={selected} isFaqSelection={faqSelected} />
          <TextField size="small" fullWidth value={rawQuery} placeholder={messages.filterTitles}
            slotProps={{ htmlInput: { 'aria-label': messages.filterTitles, type: 'search' } }}
            onChange={(event) => setFilterState({
              language,
              forSelection: selectionKey,
              rawQuery: event.target.value,
              searchExpanded: {},
            })}
            sx={{ '& .MuiOutlinedInput-root': { borderRadius: '10px', fontSize: 14 } }} />
        </Box>
        <Box sx={{ pb: 2 }}>
          {visibleTree.map((item) => (
            <SidebarItem key={item.type === 'folder' ? item.key : item.entry.key}
              item={item} language={language} selected={selected} onSelect={onSelect}
              expanded={query ? activeSearchExpanded : effectiveExpanded} defaultExpanded={Boolean(query)}
              onToggle={(key, nextOpen) => {
                if (query) {
                  setFilterState({
                    language,
                    forSelection: selectionKey,
                    rawQuery,
                    searchExpanded: { ...activeSearchExpanded, [key]: nextOpen },
                  })
                } else {
                  handleToggleFolder(key, nextOpen)
                }
              }} />
          ))}
          {visibleTree.length === 0 && <Typography role="status" sx={{ px: 2.75, py: 1, fontSize: 14, color: 'text.secondary' }}>{messages.noMatchingTitles}</Typography>}
        </Box>
      </Box>
    </Drawer>
  )
}
