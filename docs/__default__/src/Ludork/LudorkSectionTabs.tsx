import { Box, Tab, Tabs } from '@mui/material'
import type { DocSection } from './ludorkDocsIndex'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import './ludorkSite.css'

type LudorkSectionTabsProps = {
  language: LanguageKey
  sections: readonly DocSection[]
  activeSectionKey: string | false
  onSelect: (sectionKey: string) => void
}

export default function LudorkSectionTabs({
  language,
  sections,
  activeSectionKey,
  onSelect,
}: LudorkSectionTabsProps) {
  return (
    <Box className="ludork-docs-tabs">
      <Tabs
        value={activeSectionKey}
        onChange={(_event, value: string) => onSelect(value)}
        variant="scrollable"
        scrollButtons="auto"
        allowScrollButtonsMobile
        aria-label={LUDORK_SITE_MESSAGES[language].docs.sections}
        sx={{
          minHeight: 44,
          '& .MuiTabs-indicator': { height: 2, borderRadius: 980 },
          '& .MuiTab-root': {
            minHeight: 44,
            minWidth: 0,
            px: 2,
            py: 0,
            color: 'text.secondary',
            fontWeight: 500,
            textTransform: 'none',
          },
          '& .Mui-selected': {
            color: 'text.primary',
            fontWeight: 600,
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
