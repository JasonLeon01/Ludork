import { useMemo, type ReactNode } from 'react'
import { createTheme, CssBaseline, ThemeProvider, useMediaQuery } from '@mui/material'

const FONT_FAMILY = '-apple-system, BlinkMacSystemFont, "PingFang SC", "Hiragino Sans GB", "Segoe UI", sans-serif'

export default function LudorkTheme({ children }: { children: ReactNode }) {
  const isDark = useMediaQuery('(prefers-color-scheme: dark)', { noSsr: true })
  const theme = useMemo(() => createTheme({
    typography: {
      fontFamily: FONT_FAMILY,
      fontWeightBold: 600,
      button: { textTransform: 'none', fontWeight: 600 },
    },
    palette: {
      mode: isDark ? 'dark' : 'light',
      primary: { main: isDark ? '#2997ff' : '#0071e3' },
      text: {
        primary: isDark ? '#f5f5f7' : '#1d1d1f',
        secondary: isDark ? '#a1a1a6' : '#6e6e73',
      },
      background: {
        default: isDark ? '#000000' : '#f5f5f7',
        paper: isDark ? '#1d1d1f' : '#ffffff',
      },
      divider: isDark ? 'rgba(255, 255, 255, 0.12)' : 'rgba(0, 0, 0, 0.08)',
      action: {
        hover: isDark ? 'rgba(255, 255, 255, 0.06)' : 'rgba(0, 0, 0, 0.04)',
        selected: isDark ? 'rgba(255, 255, 255, 0.1)' : 'rgba(0, 0, 0, 0.05)',
      },
    },
    shape: { borderRadius: 12 },
    components: {
      MuiButton: {
        styleOverrides: {
          root: { borderRadius: 980 },
        },
      },
      MuiOutlinedInput: {
        styleOverrides: {
          root: { borderRadius: 980 },
        },
      },
      MuiIconButton: {
        styleOverrides: {
          root: { borderRadius: 980 },
        },
      },
    },
  }), [isDark])

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline enableColorScheme />
      {children}
    </ThemeProvider>
  )
}
