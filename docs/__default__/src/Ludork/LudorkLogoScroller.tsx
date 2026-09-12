import { useEffect, useRef, useState, type CSSProperties } from 'react'

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
    return () => observer.disconnect()
  }, [items])

  const trackStyle = {
    '--logo-shift': `${-layout.distance}px`,
    '--logo-duration': `${layout.distance / 32}s`,
  } as CSSProperties

  return (
    <div ref={viewportRef} className="ludork-logo-scroll" data-overflow={layout.overflowing}>
      <div className="ludork-logo-track" style={trackStyle}>
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
