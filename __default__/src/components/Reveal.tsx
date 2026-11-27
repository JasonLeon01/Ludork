import { useEffect, useRef, type ReactNode } from 'react'
import { MEDIA_QUERIES, THEME_EASE } from '../styles/breakpoints'

type RevealProps = {
  children: ReactNode
  className?: string
}

export default function Reveal({ children, className }: RevealProps) {
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const element = ref.current
    if (!element || !('IntersectionObserver' in window)) return

    const reducedMotion = window.matchMedia(MEDIA_QUERIES.reducedMotion)
    let animation: Animation | undefined
    const cancelAnimation = () => animation?.cancel()
    const observer = new IntersectionObserver((entries) => {
      if (!entries.some((entry) => entry.isIntersecting)) return
      observer.disconnect()
      if (!reducedMotion.matches) {
        animation = element.animate(
          [{ opacity: 0, transform: 'translateY(12px)' }, { opacity: 1, transform: 'translateY(0)' }],
          { duration: 400, easing: THEME_EASE },
        )
      }
    }, { threshold: 0.08 })

    observer.observe(element)
    reducedMotion.addEventListener('change', cancelAnimation)
    element.addEventListener('focusin', cancelAnimation)
    return () => {
      observer.disconnect()
      cancelAnimation()
      reducedMotion.removeEventListener('change', cancelAnimation)
      element.removeEventListener('focusin', cancelAnimation)
    }
  }, [])

  return <div ref={ref} className={className}>{children}</div>
}
