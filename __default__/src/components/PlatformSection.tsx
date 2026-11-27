import type { LudorkPlatform } from '../data/platforms'
import Reveal from './Reveal'

type PlatformSectionProps = {
  id: string
  title: string
  platforms: readonly LudorkPlatform[]
}

export default function PlatformSection({ id, title, platforms }: PlatformSectionProps) {
  return (
    <section className="ludork-platform-section" aria-labelledby={id}>
      <Reveal>
        <h2 id={id}>{title}</h2>
        <ul className="ludork-platform-icons">
          {platforms.map((platform) => {
            const iconWellClass = [
              'ludork-platform-chip-icon',
              platform.wordmark ? 'ludork-platform-chip-icon--wordmark' : '',
              platform.wide ? 'ludork-platform-chip-icon--wide' : '',
            ].filter(Boolean).join(' ')
            const chipBody = (
              <>
                <span className={iconWellClass}>
                  <img
                    src={platform.icon}
                    width={platform.wide ? 96 : platform.wordmark ? 36 : 20}
                    height={20}
                    alt={platform.wordmark ? platform.name : ''}
                    loading="lazy"
                  />
                </span>
                {!platform.wordmark && <span className="ludork-platform-chip-label">{platform.name}</span>}
              </>
            )
            return (
              <li key={platform.name} title={platform.name}>
                {platform.website ? (
                  <a className="ludork-platform-chip ludork-platform-chip--link" href={platform.website} aria-label={platform.name}>
                    {chipBody}
                  </a>
                ) : (
                  <span className="ludork-platform-chip">
                    {chipBody}
                  </span>
                )}
              </li>
            )
          })}
        </ul>
      </Reveal>
    </section>
  )
}
