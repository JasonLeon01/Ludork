import { FormControl, MenuItem, Select } from '@mui/material'
import { isFaqDoc, resolveFilenameByDocKey, type SelectedDoc } from './ludorkDocsIndex'
import type { LanguageKey } from './ludorkLanguages'
import { getSiteRoot } from './ludorkSite'
import { LUDORK_SITE_MESSAGES } from './ludorkSiteMessages'
import { getDocsVersion, LUDORK_VERSIONS } from './ludorkVersions'

export default function LudorkVersionSelect({ language, selected }: { language: LanguageKey; selected: SelectedDoc }) {
  return (
    <FormControl size="small" fullWidth>
      <Select value={getDocsVersion()} inputProps={{ 'aria-label': LUDORK_SITE_MESSAGES[language].version }}
        sx={{ borderRadius: '10px', fontSize: 14 }}
        onChange={(event) => {
          const version = event.target.value
          const url = new URL(`docs/v${version}/`, getSiteRoot())
          url.search = window.location.search
          url.searchParams.set('lang', language)
          url.searchParams.delete('version')
          url.searchParams.delete('path')
          if (selected.type === 'doc' && resolveFilenameByDocKey(language, selected.docKey, version)) {
            url.searchParams.set('doc', selected.docKey)
            if (!isFaqDoc(language, selected.docKey)) url.hash = window.location.hash
          } else {
            url.searchParams.delete('doc')
          }
          window.location.assign(url)
        }}>
        {LUDORK_VERSIONS.map((version) => <MenuItem key={version} value={version}>Ludork {version}</MenuItem>)}
      </Select>
    </FormControl>
  )
}
