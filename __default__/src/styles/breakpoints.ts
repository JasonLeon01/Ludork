export const BREAKPOINTS = {
  mobile: 600,
  tablet: 900,
  desktop: 1200,
} as const

export const MEDIA_QUERIES = {
  narrowNavigation: `(max-width: ${BREAKPOINTS.desktop - 0.05}px)`,
  mobilePortrait: `(max-width: ${BREAKPOINTS.tablet - 0.05}px) and (orientation: portrait)`,
  darkColorScheme: '(prefers-color-scheme: dark)',
  reducedMotion: '(prefers-reduced-motion: reduce)',
} as const

export const THEME_FONT_FAMILY =
  '-apple-system, BlinkMacSystemFont, "PingFang SC", "Hiragino Sans GB", "Segoe UI", sans-serif'

export const THEME_EASE = 'cubic-bezier(0.22, 1, 0.36, 1)'

export const THEME_PALETTE = {
  light: {
    primary: '#0071e3',
    textPrimary: '#1d1d1f',
    textSecondary: '#6e6e73',
    backgroundDefault: '#f5f5f7',
    backgroundPaper: '#ffffff',
    divider: 'rgba(0, 0, 0, 0.08)',
    actionHover: 'rgba(0, 0, 0, 0.04)',
    actionSelected: 'rgba(0, 0, 0, 0.05)',
  },
  dark: {
    primary: '#2997ff',
    textPrimary: '#f5f5f7',
    textSecondary: '#a1a1a6',
    backgroundDefault: '#000000',
    backgroundPaper: '#1d1d1f',
    divider: 'rgba(255, 255, 255, 0.12)',
    actionHover: 'rgba(255, 255, 255, 0.06)',
    actionSelected: 'rgba(255, 255, 255, 0.1)',
  },
} as const
