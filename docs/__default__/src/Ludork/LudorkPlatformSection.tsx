import type { LudorkPlatform } from './ludorkPlatforms'
import LudorkReveal from './LudorkReveal'

type LudorkPlatformSectionProps = {
  id: string
  title: string
  platforms: readonly LudorkPlatform[]
}

export default function LudorkPlatformSection({ id, title, platforms }: LudorkPlatformSectionProps) {
  return (
    <section className="ludork-platform-section" aria-labelledby={id}>
      <LudorkReveal>
        <h2 id={id}>{title}</h2>
        <ul className="ludork-platform-icons">
          {platforms.map((platform) => (
            <li key={platform.name} title={platform.name}>
              <img src={platform.icon} width={platform.wide ? 80 : 32} height="32" alt={platform.name} loading="lazy" />
            </li>
          ))}
        </ul>
      </LudorkReveal>
    </section>
  )
}
