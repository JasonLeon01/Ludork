import { StrictMode, type ReactNode } from 'react'
import { createRoot } from 'react-dom/client'
import Theme from '../app/Theme'

export function mountPage(node: ReactNode): void {
  createRoot(document.getElementById('root')!).render(
    <StrictMode>
      <Theme>
        {node}
      </Theme>
    </StrictMode>,
  )
}
