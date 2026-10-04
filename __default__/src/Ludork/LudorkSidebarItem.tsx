import { useId, useState } from 'react'
import { Box, ListItemButton, ListItemText } from '@mui/material'
import { ChevronLeftIcon } from './LudorkIcon'
import type { DocTreeItem, SelectedDoc } from './ludorkDocsIndex'
import type { LanguageKey } from './ludorkLanguages'
import { getLudorkDocHref } from './ludorkUrl'

type LudorkSidebarItemProps = {
  item: DocTreeItem
  language: LanguageKey
  selected: SelectedDoc
  onSelect: (doc: SelectedDoc) => void
  depth?: number
}

const labelStyle = {
  fontSize: 14,
  lineHeight: 1.55,
  whiteSpace: 'normal',
  overflowWrap: 'anywhere',
}

export default function LudorkSidebarItem({
  item,
  language,
  selected,
  onSelect,
  depth = 0,
}: LudorkSidebarItemProps) {
  if (item.type === 'folder') {
    return <LudorkSidebarFolder item={item} language={language} selected={selected} onSelect={onSelect} depth={depth} />
  }

  const docKey = item.entry.key
  const isSelected = selected.type === 'doc' && selected.lang === language && selected.docKey === docKey

  return (
    <ListItemButton
      component="a"
      href={getLudorkDocHref(language, docKey)}
      selected={isSelected}
      aria-current={isSelected ? 'page' : undefined}
      onClick={(event) => {
        if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
        event.preventDefault()
        onSelect({ type: 'doc', lang: language, docKey })
      }}
      sx={{ pl: 1.5 + depth * 1.5 }}
    >
      <ListItemText
        primary={item.entry.displayName}
        slotProps={{ primary: { sx: { ...labelStyle, fontWeight: isSelected ? 600 : 400 } } }}
      />
    </ListItemButton>
  )
}

function LudorkSidebarFolder({
  item,
  language,
  selected,
  onSelect,
  depth = 0,
}: LudorkSidebarItemProps & { item: Extract<DocTreeItem, { type: 'folder' }> }) {
  const childrenId = useId()
  const selectionKey = selected.type === 'doc' ? `${selected.lang}:${selected.docKey}` : `${language}:home`
  const [state, setState] = useState({ selectionKey, expanded: true })
  let expanded = state.expanded

  if (state.selectionKey !== selectionKey) {
    if (selected.type === 'doc' && selected.lang === language && containsDoc(item, selected.docKey)) {
      expanded = true
    }
    setState({ selectionKey, expanded })
  }

  return (
    <Box>
      <ListItemButton
        component="button"
        type="button"
        aria-expanded={expanded}
        aria-controls={childrenId}
        onClick={() => setState({ selectionKey, expanded: !expanded })}
        sx={{ pl: 1.5 + depth * 1.5, width: 'calc(100% - 24px)', textAlign: 'left', gap: 1 }}
      >
        <ListItemText primary={item.displayName} slotProps={{ primary: { sx: { ...labelStyle, fontWeight: 400 } } }} />
        <Box component="span" sx={{ display: 'inline-flex', flexShrink: 0, transform: expanded ? 'rotate(-90deg)' : 'rotate(180deg)' }}>
          <ChevronLeftIcon size={18} />
        </Box>
      </ListItemButton>
      <Box id={childrenId} hidden={!expanded}>
        {item.children.map((child) => (
          <LudorkSidebarItem
            key={child.type === 'folder' ? child.path : child.entry.key}
            item={child}
            language={language}
            selected={selected}
            onSelect={onSelect}
            depth={depth + 1}
          />
        ))}
      </Box>
    </Box>
  )
}

function containsDoc(item: DocTreeItem, docKey: string): boolean {
  return item.type === 'doc'
    ? item.entry.key === docKey
    : item.children.some((child) => containsDoc(child, docKey))
}
