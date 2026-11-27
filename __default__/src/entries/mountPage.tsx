import { StrictMode, type ReactNode } from 'react'
import { createRoot } from 'react-dom/client'
import Theme from '../app/Theme'
import { applyThemePreference, readThemePreference } from '../app/themePreference'

export function mountPage(node: ReactNode): void {
  applyThemePreference(readThemePreference())
  createRoot(document.getElementById('root')!).render(
    <StrictMode>
      <Theme>
        {node}
      </Theme>
    </StrictMode>,
  )
}
