import {
  Box,
  IconButton,
  List,
  ListItemButton,
  ListItemText,
  Typography,
} from '@mui/material'
import {
  type DocSection,
  type SelectedDoc,
} from './ludorkDocsIndex'
import { ChevronLeftIcon, HomeIcon } from './LudorkIcon'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { getLudorkDocHref } from './ludorkUrl'
import LudorkSidebarItem from './LudorkSidebarItem'

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
        bgcolor: 'transparent',
      }}
    >
      <Box sx={{ px: 2.75, pt: 2.75, pb: 1.75, display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexShrink: 0 }}>
        <Typography variant="h6" sx={{ fontSize: 17, fontWeight: 600, letterSpacing: '-0.025em' }}>
          Ludork
        </Typography>
        {onToggle && (
          <IconButton onClick={onToggle} size="small" aria-label={LUDORK_SITE_MESSAGES[language].docs.collapseSidebar}>
            <ChevronLeftIcon />
          </IconButton>
        )}
      </Box>

      <List dense disablePadding sx={{ flex: '1 0 auto', pb: 3 }}>
        {section.includesHome && (
          <ListItemButton
            component="a"
            href={getLudorkDocHref(language, null)}
            selected={selected.type === 'home'}
            aria-current={selected.type === 'home' ? 'page' : undefined}
            onClick={(event) => {
              if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
              event.preventDefault()
              onSelect({ type: 'home' })
            }}
            sx={{ pl: 1.5, gap: 1, alignItems: 'flex-start', '& > span': { flexShrink: 0, mt: '5px' } }}
          >
            <HomeIcon />
            <ListItemText
              primary={section.displayName}
              slotProps={{ primary: { sx: { fontSize: 14, fontWeight: selected.type === 'home' ? 600 : 400 } } }}
            />
          </ListItemButton>
        )}

        {section.items.map((item) => (
          <LudorkSidebarItem
            key={item.type === 'folder' ? item.path : item.entry.key}
            item={item}
            language={language}
            selected={selected}
            onSelect={onSelect}
          />
        ))}
      </List>
    </Box>
  )
}
