import type { ReactNode } from 'react'
import {
  Box,
  IconButton,
  List,
  ListItemButton,
  ListItemText,
  Typography,
} from '@mui/material'
import {
  docKeyFromFilename,
  type DocSection,
  type DocTreeItem,
  type SelectedDoc,
} from './ludorkDocsIndex'
import { ChevronLeftIcon, HomeIcon } from './LudorkIcon'
import type { LanguageKey } from './ludorkLanguages'

type LudorkSidebarProps = {
  language: LanguageKey
  section: DocSection
  selected: SelectedDoc
  onSelect: (doc: SelectedDoc) => void
  onToggle?: () => void
}

export default function LudorkSidebar({
  language,
  section,
  selected,
  onSelect,
  onToggle,
}: LudorkSidebarProps) {
  return (
    <Box
      className="ludork-sidebar"
      sx={{
        width: 280,
        height: '100%',
        overflow: 'auto',
        display: 'flex',
        flexDirection: 'column',
        bgcolor: 'background.paper',
        whiteSpace: 'nowrap',
      }}
    >
      <Box sx={{ px: 2, pt: 2, pb: 1, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <Typography variant="h6" sx={{ fontWeight: 700 }}>
          Ludork
        </Typography>
        {onToggle && (
          <IconButton onClick={onToggle} size="small" aria-label="Collapse sidebar">
            <ChevronLeftIcon />
          </IconButton>
        )}
      </Box>

      <List dense disablePadding sx={{ flex: 1, overflow: 'auto' }}>
        {section.includesHome && (
          <ListItemButton
            selected={selected.type === 'home'}
            onClick={() => onSelect({ type: 'home' })}
            sx={{ pl: 2, gap: 1 }}
          >
            <HomeIcon />
            <ListItemText
              primary={section.displayName}
              slotProps={{ primary: { sx: { fontSize: 14, fontWeight: selected.type === 'home' ? 600 : 400 } } }}
            />
          </ListItemButton>
        )}

        {section.items.map((item) => renderTreeItem(item, language, selected, onSelect))}
      </List>
    </Box>
  )
}

function renderTreeItem(
  item: DocTreeItem,
  language: LanguageKey,
  selected: SelectedDoc,
  onSelect: (doc: SelectedDoc) => void,
  depth = 0,
): ReactNode {
  if (item.type === 'folder') {
    return (
      <Box key={`folder-${item.path}`}>
        <Typography
          variant="caption"
          sx={{
            display: 'block',
            pl: 2 + depth * 2,
            pr: 2,
            pt: depth === 0 ? 1.25 : 0.75,
            pb: 0.25,
            color: 'text.secondary',
            fontWeight: 700,
            overflow: 'hidden',
            textOverflow: 'ellipsis',
          }}
        >
          {item.displayName}
        </Typography>
        {item.children.map((child) => renderTreeItem(child, language, selected, onSelect, depth + 1))}
      </Box>
    )
  }

  const docKey = docKeyFromFilename(item.entry.filename)
  const isSelected =
    selected.type === 'doc' &&
    selected.lang === language &&
    selected.docKey === docKey

  return (
    <ListItemButton
      key={item.entry.filename}
      selected={isSelected}
      onClick={() => onSelect({ type: 'doc', lang: language, docKey })}
      sx={{ pl: 2 + depth * 2 }}
    >
      <ListItemText
        primary={item.entry.displayName}
        slotProps={{
          primary: {
            sx: {
              fontSize: 14,
              fontWeight: isSelected ? 600 : 400,
              overflow: 'hidden',
              textOverflow: 'ellipsis',
            },
          },
        }}
      />
    </ListItemButton>
  )
}
