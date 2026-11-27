import { FormControl, MenuItem, Select } from '@mui/material'
import { getDocsRoute } from '../../../docs_versions.mjs'
import { getSiteRoot } from '../data/site'
import type { SelectedDoc } from '../docs/docsIndex'
import { getDocsVersion, LUDORK_VERSIONS } from '../docs/docsVersions'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'

type VersionSelectProps = {
  language: LanguageKey
  selected: SelectedDoc
  isFaqSelection: boolean
}

export default function VersionSelect({ language, selected, isFaqSelection }: VersionSelectProps) {
  return (
    <FormControl size="small" fullWidth>
      <Select value={getDocsVersion()} inputProps={{ 'aria-label': LUDORK_SITE_MESSAGES[language].version }}
        sx={{ borderRadius: '10px', fontSize: 14 }}
        onChange={(event) => {
          const version = event.target.value
          const url = new URL(getDocsRoute(version), getSiteRoot())
          url.search = window.location.search
          url.searchParams.set('lang', language)
          url.searchParams.delete('version')
          url.searchParams.delete('path')
          if (selected.type === 'doc') {
            url.searchParams.set('doc', selected.docKey)
            if (!isFaqSelection) url.hash = window.location.hash
          } else {
            url.searchParams.delete('doc')
          }
          window.location.assign(url)
        }}>
        {LUDORK_VERSIONS.map((version) => (
          <MenuItem key={version} value={version}>
            {version === 'trunk' ? LUDORK_SITE_MESSAGES[language].developmentVersion : `Ludork ${version}`}
          </MenuItem>
        ))}
      </Select>
    </FormControl>
  )
}
