import { FormControl, InputAdornment, MenuItem, Select } from '@mui/material'
import { LUDORK_LANGUAGES, LUDORK_LANGUAGE_KEYS, type LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'
import { GlobeIcon } from './Icons'

export default function LanguageSelect({ language, onChange }: {
  language: LanguageKey
  onChange: (language: LanguageKey) => void
}) {
  return (
    <FormControl size="small" className="ludork-language">
      <Select value={language} onChange={(event) => onChange(event.target.value as LanguageKey)}
        inputProps={{ 'aria-label': LUDORK_SITE_MESSAGES[language].language }}
        startAdornment={<InputAdornment position="start"><GlobeIcon size={18} /></InputAdornment>}
        sx={{
          borderRadius: '980px',
          '& .MuiSelect-select': { py: 0.85, fontWeight: 500 },
          '&& .MuiSelect-select.MuiInputBase-input': { paddingRight: '2.25rem' },
        }}>
        {LUDORK_LANGUAGE_KEYS.map((key) => <MenuItem key={key} value={key}>{LUDORK_LANGUAGES[key].label}</MenuItem>)}
      </Select>
    </FormControl>
  )
}
