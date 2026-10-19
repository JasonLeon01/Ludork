import { useEffect, useMemo, useRef, useState } from 'react'
import { Box, Divider, Drawer, IconButton, ListItemButton, ListItemText, TextField, Typography, useMediaQuery } from '@mui/material'
import { getDocsFaqTree, isFaqDoc, type DocSection, type DocTreeItem, type SelectedDoc } from './ludorkDocsIndex'
import { ChevronLeftIcon } from './LudorkIcon'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { getLudorkDocHref } from './ludorkUrl'
import LudorkSidebarItem from './LudorkSidebarItem'
import LudorkVersionSelect from './LudorkVersionSelect'

type LudorkSidebarProps = {
  language: LanguageKey
  sections: readonly DocSection[]
  selected: SelectedDoc
  onSelect: (doc: SelectedDoc) => void
  isNarrow: boolean
  open: boolean
  onClose: () => void
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

export default function LudorkSidebar({ language, sections, selected, onSelect, isNarrow, open, onClose }: LudorkSidebarProps) {
  const sidebarRef = useRef<HTMLElement>(null)
  const reducedMotion = useMediaQuery('(prefers-reduced-motion: reduce)')
  const messages = LUDORK_SITE_MESSAGES[language].docs
  const faqTree = useMemo(() => getDocsFaqTree(language), [language])
  const faqSelected = selected.type === 'doc' && isFaqDoc(language, selected.docKey)
  const tree = useMemo<DocTreeItem[]>(() => sections.map((section) => ({
    type: 'folder', key: section.key, displayName: section.displayName, children: section.items,
  })), [sections])
  const selectionKey = selected.type === 'doc' ? selected.docKey : 'home'
  const ancestors = selected.type === 'doc' ? selectedAncestors([faqTree, ...tree], selected.docKey) ?? [] : []
  const [state, setState] = useState(() => ({
    language, selectionKey, query: '',
    expanded: Object.fromEntries(ancestors.map((key) => [key, true])) as Record<string, boolean>,
    searchExpanded: {} as Record<string, boolean>,
  }))
  if (state.selectionKey !== selectionKey || state.language !== language) {
    setState({
      ...state, language, selectionKey,
      query: state.language === language && (selected.type === 'home' || faqSelected
        || selectedAncestors(filterItems(tree, state.query.trim().toLocaleLowerCase()), selectionKey)) ? state.query : '',
      expanded: { ...state.expanded, ...Object.fromEntries(ancestors.map((key) => [key, true])) },
      searchExpanded: {},
    })
  }
  const query = state.query.trim().toLocaleLowerCase()
  const visibleTree = query ? filterItems(tree, query) : tree
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
          <LudorkSidebarItem item={{ ...faqTree, displayName: messages.faq }} language={language} selected={selected}
            onSelect={onSelect} expanded={state.expanded} defaultExpanded={false}
            onToggle={(key, expanded) => setState((previous) => ({ ...previous, expanded: { ...previous.expanded, [key]: expanded } }))} />
        </Box>
        <Divider sx={{ mx: 2 }} />
        <Box sx={{ px: 2, py: 1.5, display: 'grid', gap: 1 }}>
          <LudorkVersionSelect language={language} selected={selected} />
          <TextField size="small" fullWidth value={state.query} placeholder={messages.filterTitles}
            slotProps={{ htmlInput: { 'aria-label': messages.filterTitles, type: 'search' } }}
            onChange={(event) => setState((previous) => ({ ...previous, query: event.target.value, searchExpanded: {} }))}
            sx={{ '& .MuiOutlinedInput-root': { borderRadius: '10px', fontSize: 14 } }} />
        </Box>
        <Box sx={{ pb: 2 }}>
          {visibleTree.map((item) => (
            <LudorkSidebarItem key={item.type === 'folder' ? item.key : item.entry.key}
              item={item} language={language} selected={selected} onSelect={onSelect}
              expanded={query ? state.searchExpanded : state.expanded} defaultExpanded={Boolean(query)}
              onToggle={(key, expanded) => setState((previous) => query
                ? { ...previous, searchExpanded: { ...previous.searchExpanded, [key]: expanded } }
                : { ...previous, expanded: { ...previous.expanded, [key]: expanded } })} />
          ))}
          {visibleTree.length === 0 && <Typography role="status" sx={{ px: 2.75, py: 1, fontSize: 14, color: 'text.secondary' }}>{messages.noMatchingTitles}</Typography>}
        </Box>
      </Box>
    </Drawer>
  )
}
