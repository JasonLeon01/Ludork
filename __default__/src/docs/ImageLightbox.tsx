import { useEffect, useRef } from 'react'
import { CloseIcon } from '../components/Icons'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'

type ImageLightboxProps = {
  src: string
  alt: string
  language: LanguageKey
  onClose: () => void
}

export default function ImageLightbox({ src, alt, language, onClose }: ImageLightboxProps) {
  const closeButtonRef = useRef<HTMLButtonElement>(null)
  const messages = LUDORK_SITE_MESSAGES[language].docs

  useEffect(() => {
    const previousFocus = document.activeElement instanceof HTMLElement ? document.activeElement : null
    closeButtonRef.current?.focus({ preventScroll: true })
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        event.preventDefault()
        onClose()
      }
    }
    window.addEventListener('keydown', onKeyDown)
    return () => {
      window.removeEventListener('keydown', onKeyDown)
      previousFocus?.focus({ preventScroll: true })
    }
  }, [onClose])

  return (
    <div
      className="ludork-lightbox"
      role="dialog"
      aria-modal="true"
      aria-label={alt || messages.zoomImage}
      onClick={(event) => {
        if (event.target === event.currentTarget) onClose()
      }}
    >
      <button
        ref={closeButtonRef}
        type="button"
        className="ludork-lightbox-close"
        onClick={onClose}
        aria-label={messages.closePreview}
      >
        <CloseIcon size={20} />
      </button>
      <figure className="ludork-lightbox-figure">
        <img src={src} alt={alt} />
        {alt && <figcaption>{alt}</figcaption>}
      </figure>
    </div>
  )
}
