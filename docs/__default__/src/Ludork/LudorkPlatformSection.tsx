import { useEffect, useRef, useState, type CSSProperties } from 'react'
import type { LudorkPlatform } from './ludorkPlatforms'

type LudorkPlatformSectionProps = {
  id: string
  title: string
  platforms: readonly LudorkPlatform[]
}

export default function LudorkPlatformSection({ id, title, platforms }: LudorkPlatformSectionProps) {
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
  }, [platforms])

  const trackStyle = {
    '--platform-shift': `${-layout.distance}px`,
    '--platform-duration': `${layout.distance / 32}s`,
  } as CSSProperties

  return (
    <section className="ludork-platform-section" aria-labelledby={id}>
      <div className="ludork-container ludork-section">
        <h2 id={id}>{title}</h2>
        <div ref={viewportRef} className="ludork-platform-scroll" data-overflow={layout.overflowing}>
          <div className="ludork-platform-track" style={trackStyle}>
            {Array.from({ length: layout.overflowing ? 2 : 1 }, (_, copy) => (
              <ul
                key={copy}
                ref={copy === 0 ? groupRef : undefined}
                className={`ludork-platform-group${copy === 1 ? ' ludork-platform-copy' : ''}`}
                aria-hidden={copy === 1 ? true : undefined}
              >
                {platforms.map((platform) => (
                  <li key={platform.name}>
                    <a
                      className={`ludork-platform${platform.wide ? ' ludork-platform-wide' : ''}`}
                      href={platform.website}
                      tabIndex={copy === 1 ? -1 : undefined}
                      onFocus={(event) => {
                        if (event.currentTarget.matches(':focus-visible')) {
                          event.currentTarget.scrollIntoView({ block: 'nearest', inline: 'nearest' })
                        }
                      }}
                    >
                      <span className="ludork-platform-icon">
                        <img src={platform.icon} width={platform.wide ? 180 : 112} height="96" alt="" loading="lazy" />
                      </span>
                      <span>{platform.name}</span>
                    </a>
                  </li>
                ))}
              </ul>
            ))}
          </div>
        </div>
      </div>
    </section>
  )
}
