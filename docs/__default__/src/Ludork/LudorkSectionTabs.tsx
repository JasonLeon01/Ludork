import { Box, Tab, Tabs } from '@mui/material'
import type { DocSection } from './ludorkDocsIndex'

type LudorkSectionTabsProps = {
  sections: readonly DocSection[]
  activeSectionKey: string | false
  onSelect: (sectionKey: string) => void
}

export default function LudorkSectionTabs({
  sections,
  activeSectionKey,
  onSelect,
}: LudorkSectionTabsProps) {
  return (
    <Box sx={{ borderBottom: 1, borderColor: 'divider', bgcolor: 'background.paper' }}>
      <Tabs
        value={activeSectionKey}
        onChange={(_event, value: string) => onSelect(value)}
        variant="scrollable"
        scrollButtons="auto"
        allowScrollButtonsMobile
        aria-label="Documentation sections"
        sx={{
          minHeight: 38,
          '& .MuiTab-root': {
            minHeight: 38,
            minWidth: 112,
            px: 2,
            py: 0,
            fontWeight: 700,
            textTransform: 'none',
          },
        }}
      >
        {sections.map((section) => (
          <Tab key={section.key} value={section.key} label={section.displayName} />
        ))}
      </Tabs>
    </Box>
  )
}
