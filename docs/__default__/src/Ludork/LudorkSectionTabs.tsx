import { Box, IconButton, Tab, Tabs } from '@mui/material'
import type { DocSection } from './ludorkDocsIndex'
import type { LanguageKey } from './ludorkLanguages'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { ChevronLeftIcon, MenuIcon } from './LudorkIcon'
import './ludorkSite.css'

type LudorkSectionTabsProps = {
  language: LanguageKey
  sections: readonly DocSection[]
  activeSectionKey: string | false
  onSelect: (sectionKey: string) => void
  onToggleNavigation?: () => void
  navigationExpanded?: boolean
}

export default function LudorkSectionTabs({
  language,
  sections,
  activeSectionKey,
  onSelect,
  onToggleNavigation,
  navigationExpanded = false,
}: LudorkSectionTabsProps) {
  const messages = LUDORK_SITE_MESSAGES[language].docs
  return (
    <Box className="ludork-docs-tabs" sx={{ display: 'flex', alignItems: 'center', gap: 0.5, px: { xs: 1, md: 2 }, flexShrink: 0 }}>
      {onToggleNavigation && (
        <IconButton
          onClick={onToggleNavigation}
          aria-label={navigationExpanded ? messages.collapseSidebar : messages.expandSidebar}
          aria-expanded={navigationExpanded}
          aria-controls="document-navigation"
          sx={{ width: 36, height: 36, flexShrink: 0, borderRadius: '10px' }}
        >
          {navigationExpanded ? <ChevronLeftIcon /> : <MenuIcon />}
        </IconButton>
      )}
      <Tabs
        value={activeSectionKey}
        onChange={(_event, value: string) => onSelect(value)}
        variant="scrollable"
        scrollButtons="auto"
        allowScrollButtonsMobile
        aria-label={messages.sections}
        sx={{
          minHeight: 52,
          minWidth: 0,
          flex: 1,
          '& .MuiTabs-indicator': { height: 2, borderRadius: 980, backgroundColor: 'var(--ludork-blue)' },
          '& .MuiTabs-scrollButtons': { width: 28 },
          '& .MuiTab-root': {
            minHeight: 52,
            minWidth: 0,
            px: { xs: 1.5, md: 2.25 },
            py: 0,
            color: 'text.secondary',
            fontSize: 13,
            fontWeight: 500,
            textTransform: 'none',
            transition: 'color 180ms var(--ludork-ease), background-color 180ms var(--ludork-ease)',
            '&:hover': { color: 'text.primary', backgroundColor: 'var(--ludork-hover)' },
          },
          '& .Mui-selected': {
            color: 'var(--ludork-blue)',
            fontWeight: 600,
          },
          '@media (prefers-reduced-motion: reduce)': {
            '& .MuiTab-root, & .MuiTabs-indicator': { transition: 'none' },
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
