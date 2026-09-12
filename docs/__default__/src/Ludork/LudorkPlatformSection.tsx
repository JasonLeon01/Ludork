import type { LudorkPlatform } from './ludorkPlatforms'
import LudorkLogoScroller from './LudorkLogoScroller'
import LudorkReveal from './LudorkReveal'

type LudorkPlatformSectionProps = {
  id: string
  title: string
  platforms: readonly LudorkPlatform[]
}

export default function LudorkPlatformSection({ id, title, platforms }: LudorkPlatformSectionProps) {
  return (
    <section className="ludork-platform-section" aria-labelledby={id}>
      <LudorkReveal className="ludork-container ludork-section">
        <h2 id={id}>{title}</h2>
        <LudorkLogoScroller items={platforms} />
      </LudorkReveal>
    </section>
  )
}
