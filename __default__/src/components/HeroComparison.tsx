import { useEffect, useRef, useState, type PointerEvent } from 'react'
import editorScreenshot from '../assets/hero/home-hero-editor.webp'
import gameScreenshot from '../assets/hero/home-hero-game.webp'

type HeroComparisonProps = {
  description: string
  editorLabel: string
  gameLabel: string
}

export default function HeroComparison({ description, editorLabel, gameLabel }: HeroComparisonProps) {
  const ref = useRef<HTMLDivElement>(null)
  const [activeSide, setActiveSide] = useState<'editor' | 'game' | null>(null)

  useEffect(() => {
    const clearOutsideSelection = (event: MouseEvent) => {
      if (event.target instanceof Node && !ref.current?.contains(event.target)) {
        setActiveSide(null)
      }
    }

    document.addEventListener('click', clearOutsideSelection, true)
    return () => document.removeEventListener('click', clearOutsideSelection, true)
  }, [])

  const updateHover = (event: PointerEvent<HTMLDivElement>) => {
    if (event.pointerType !== 'mouse') return

    const bounds = event.currentTarget.getBoundingClientRect()
    const x = (event.clientX - bounds.left) / bounds.width
    const y = (event.clientY - bounds.top) / bounds.height
    setActiveSide(x >= y ? 'game' : 'editor')
  }

  return (
    <div
      ref={ref}
      className="ludork-hero-comparison"
      role="group"
      aria-label={description}
      data-active={activeSide ?? undefined}
      onPointerEnter={updateHover}
      onPointerMove={updateHover}
      onPointerLeave={(event) => {
        if (event.pointerType === 'mouse') setActiveSide(null)
      }}
      onBlur={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget)) setActiveSide(null)
      }}
      onKeyDown={(event) => {
        if (event.key === 'Escape') setActiveSide(null)
      }}
    >
      <div className="ludork-hero-layer ludork-hero-layer--editor" aria-hidden="true">
        <img src={editorScreenshot} alt="" width="832" height="831" fetchPriority="high" decoding="async" draggable={false} />
      </div>
      <div className="ludork-hero-layer ludork-hero-layer--game" aria-hidden="true">
        <img src={gameScreenshot} alt="" width="1248" height="1246" fetchPriority="high" decoding="async" draggable={false} />
      </div>
      <div className="ludork-hero-seam" aria-hidden="true" />
      <button
        type="button"
        className="ludork-hero-hit-area ludork-hero-hit-area--editor"
        aria-label={editorLabel}
        aria-pressed={activeSide === 'editor'}
        onClick={() => setActiveSide('editor')}
      />
      <button
        type="button"
        className="ludork-hero-hit-area ludork-hero-hit-area--game"
        aria-label={gameLabel}
        aria-pressed={activeSide === 'game'}
        onClick={() => setActiveSide('game')}
      />
    </div>
  )
}
