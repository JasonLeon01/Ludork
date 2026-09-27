import { useEffect, useRef, useState } from 'react'

export type LudorkLogoItem = {
  name: string
  website: string
  icon?: string
  wide?: boolean
}

type LudorkLogoScrollerProps = {
  items: readonly LudorkLogoItem[]
}

export default function LudorkLogoScroller({ items }: LudorkLogoScrollerProps) {
  const viewportRef = useRef<HTMLDivElement>(null)
  const groupRef = useRef<HTMLUListElement>(null)
  const interactionRef = useRef({ touching: false, pauseUntil: 0 })
  const [layout, setLayout] = useState({ overflowing: false, distance: 0 })

  useEffect(() => {
    const viewport = viewportRef.current
    const group = groupRef.current
    if (!viewport || !group) return

    const observer = new ResizeObserver(() => {
      const distance = group.getBoundingClientRect().width
      const endGap = parseFloat(getComputedStyle(group).paddingRight)
      const overflowing = distance - endGap > viewport.clientWidth + 1
      setLayout((previous) => previous.overflowing === overflowing && previous.distance === distance
        ? previous : { overflowing, distance })
    })
    observer.observe(viewport)
    observer.observe(group)

    const onWheel = (event: WheelEvent) => {
      if (event.ctrlKey || (event.deltaX === 0 && (!event.shiftKey || event.deltaY === 0))) return
      if (viewport.scrollWidth <= viewport.clientWidth) return

      interactionRef.current.pauseUntil = performance.now() + 1000
      if (event.deltaX !== 0) return

      const unit = event.deltaMode === WheelEvent.DOM_DELTA_LINE ? 16
        : event.deltaMode === WheelEvent.DOM_DELTA_PAGE ? viewport.clientWidth : 1
      event.preventDefault()
      viewport.scrollLeft += event.deltaY * unit
    }
    const onTouchStart = () => {
      interactionRef.current.touching = true
    }
    const onTouchEnd = (event: TouchEvent) => {
      interactionRef.current.touching = event.touches.length > 0
      interactionRef.current.pauseUntil = performance.now() + 1000
    }
    viewport.addEventListener('wheel', onWheel, { passive: false })
    viewport.addEventListener('touchstart', onTouchStart, { passive: true })
    viewport.addEventListener('touchend', onTouchEnd, { passive: true })
    viewport.addEventListener('touchcancel', onTouchEnd, { passive: true })
    return () => {
      observer.disconnect()
      viewport.removeEventListener('wheel', onWheel)
      viewport.removeEventListener('touchstart', onTouchStart)
      viewport.removeEventListener('touchend', onTouchEnd)
      viewport.removeEventListener('touchcancel', onTouchEnd)
    }
  }, [items])

  useEffect(() => {
    const viewport = viewportRef.current
    if (!viewport || !layout.overflowing || layout.distance <= 0) return

    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)')
    const hover = window.matchMedia('(hover: hover)')
    let frame = 0
    let previousTime = 0
    let position = viewport.scrollLeft
    let lastScrollLeft = position

    const advance = (time: number) => {
      const interaction = interactionRef.current
      if (viewport.scrollLeft !== lastScrollLeft) interaction.pauseUntil = time + 1000

      const paused = interaction.touching || time < interaction.pauseUntil
        || (hover.matches && viewport.matches(':hover')) || viewport.matches(':focus-within, :active')
      if (paused) {
        position = viewport.scrollLeft
      } else if (previousTime) {
        position = (position + Math.min(time - previousTime, 64) * 32 / 1000) % layout.distance
        viewport.scrollLeft = position
      }
      lastScrollLeft = viewport.scrollLeft
      previousTime = time
      frame = requestAnimationFrame(advance)
    }

    const updatePlayback = () => {
      cancelAnimationFrame(frame)
      previousTime = 0
      position = viewport.scrollLeft
      lastScrollLeft = position
      if (!reducedMotion.matches) frame = requestAnimationFrame(advance)
    }
    updatePlayback()
    reducedMotion.addEventListener('change', updatePlayback)
    return () => {
      cancelAnimationFrame(frame)
      reducedMotion.removeEventListener('change', updatePlayback)
    }
  }, [layout])

  return (
    <div ref={viewportRef} className="ludork-logo-scroll" data-overflow={layout.overflowing}>
      <div className="ludork-logo-track">
        {Array.from({ length: layout.overflowing ? 2 : 1 }, (_, copy) => (
          <ul
            key={copy}
            ref={copy === 0 ? groupRef : undefined}
            className={`ludork-logo-group${copy === 1 ? ' ludork-logo-copy' : ''}`}
            aria-hidden={copy === 1 ? true : undefined}
          >
            {items.map((item) => (
              <li key={item.name}>
                <a
                  className={`ludork-logo-card${item.wide ? ' ludork-logo-wide' : ''}`}
                  href={item.website}
                  tabIndex={copy === 1 ? -1 : undefined}
                  onFocus={(event) => {
                    if (event.currentTarget.matches(':focus-visible')) {
                      event.currentTarget.scrollIntoView({ block: 'nearest', inline: 'nearest' })
                    }
                  }}
                >
                  {item.icon ? (
                    <>
                      <span className="ludork-logo-icon">
                        <img src={item.icon} width={item.wide ? 180 : 112} height="96" alt="" loading="lazy" />
                      </span>
                      <span>{item.name}</span>
                    </>
                  ) : <span className="ludork-logo-wordmark">{item.name}</span>}
                </a>
              </li>
            ))}
          </ul>
        ))}
      </div>
    </div>
  )
}
