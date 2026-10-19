import { useId } from 'react'
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
  expanded: Readonly<Record<string, boolean>>
  defaultExpanded: boolean
  onToggle: (key: string, expanded: boolean) => void
  depth?: number
}

const labelStyle = { fontSize: 14, lineHeight: 1.55, whiteSpace: 'normal', overflowWrap: 'anywhere' }

export default function LudorkSidebarItem({
  item, language, selected, onSelect, expanded, defaultExpanded, onToggle, depth = 0,
}: LudorkSidebarItemProps) {
  const childrenId = useId()
  if (item.type === 'folder') {
    const open = expanded[item.key] ?? defaultExpanded
    return (
      <Box>
        <ListItemButton component="button" type="button" aria-expanded={open} aria-controls={childrenId}
          onClick={() => onToggle(item.key, !open)}
          sx={{ pl: 1.5 + depth * 1.5, width: 'calc(100% - 24px)', textAlign: 'left', gap: 1 }}>
          <Box component="span" sx={{ display: 'inline-flex', flexShrink: 0, transform: open ? 'rotate(-90deg)' : 'rotate(180deg)' }}>
            <ChevronLeftIcon size={16} />
          </Box>
          <ListItemText primary={item.displayName} slotProps={{ primary: { sx: { ...labelStyle, fontWeight: depth === 0 ? 500 : 400 } } }} />
        </ListItemButton>
        <Box id={childrenId} hidden={!open}>
          {item.children.map((child) => (
            <LudorkSidebarItem key={child.type === 'folder' ? child.key : child.entry.key}
              item={child} language={language} selected={selected} onSelect={onSelect}
              expanded={expanded} defaultExpanded={defaultExpanded} onToggle={onToggle} depth={depth + 1} />
          ))}
        </Box>
      </Box>
    )
  }

  const docKey = item.entry.key
  const isSelected = selected.type === 'doc' && selected.lang === language && selected.docKey === docKey
  return (
    <ListItemButton component="a" href={getLudorkDocHref(language, docKey)}
      selected={isSelected} aria-current={isSelected ? 'page' : undefined}
      onClick={(event) => {
        if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
        event.preventDefault()
        onSelect({ type: 'doc', lang: language, docKey })
      }} sx={{ pl: 1.5 + depth * 1.5 }}>
      <ListItemText primary={item.entry.displayName}
        slotProps={{ primary: { sx: { ...labelStyle, fontWeight: isSelected ? 600 : 400 } } }} />
    </ListItemButton>
  )
}
