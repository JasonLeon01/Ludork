import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import LudorkRoot from './Ludork/LudorkRoot'
import LudorkTheme from './Ludork/LudorkTheme'
import './index.css'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <LudorkTheme>
      <LudorkRoot />
    </LudorkTheme>
  </StrictMode>,
)
