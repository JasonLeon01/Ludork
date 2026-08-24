import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import LudorkApp from './Ludork/LudorkApp'
import './index.css'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <LudorkApp />
  </StrictMode>,
)

