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
            const icon = (
              <img src={platform.icon} width={platform.wide ? 80 : 32} height="32" alt={platform.name} loading="lazy" />
            )
            return (
              <li key={platform.name} title={platform.name}>
                {platform.website ? (
                  <a href={platform.website} aria-label={platform.name}>
                    {icon}
                  </a>
                ) : icon}
              </li>
            )
          })}
        </ul>
      </Reveal>
    </section>
  )
}
