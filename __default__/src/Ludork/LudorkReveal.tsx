import { useEffect, useRef, type ReactNode } from 'react'

type LudorkRevealProps = {
  children: ReactNode
  className?: string
}

export default function LudorkReveal({ children, className }: LudorkRevealProps) {
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const element = ref.current
    if (!element || !('IntersectionObserver' in window)) return

    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)')
    let animation: Animation | undefined
    const cancelAnimation = () => animation?.cancel()
    const observer = new IntersectionObserver((entries) => {
      if (!entries.some((entry) => entry.isIntersecting)) return
      observer.disconnect()
      if (!reducedMotion.matches) {
        animation = element.animate(
          [{ opacity: 0, transform: 'translateY(12px)' }, { opacity: 1, transform: 'translateY(0)' }],
          { duration: 400, easing: 'cubic-bezier(0.22, 1, 0.36, 1)' },
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
