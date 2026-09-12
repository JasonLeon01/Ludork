import { useMemo, type ReactNode } from 'react'
import { createTheme, CssBaseline, ThemeProvider, useMediaQuery } from '@mui/material'

const FONT_FAMILY = '-apple-system, BlinkMacSystemFont, "PingFang SC", "Hiragino Sans GB", "Segoe UI", sans-serif'

export default function LudorkTheme({ children }: { children: ReactNode }) {
  const isDark = useMediaQuery('(prefers-color-scheme: dark)', { noSsr: true })
  const reducedMotion = useMediaQuery('(prefers-reduced-motion: reduce)', { noSsr: true })
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
    transitions: {
      easing: { easeOut: 'cubic-bezier(0.22, 1, 0.36, 1)' },
      duration: {
        shortest: reducedMotion ? 0 : 160,
        shorter: reducedMotion ? 0 : 180,
        short: reducedMotion ? 0 : 200,
        standard: reducedMotion ? 0 : 200,
        complex: reducedMotion ? 0 : 220,
        enteringScreen: reducedMotion ? 0 : 200,
        leavingScreen: reducedMotion ? 0 : 180,
      },
    },
    components: {
      MuiButtonBase: {
        defaultProps: { disableRipple: true },
        styleOverrides: {
          root: {
            '&.Mui-focusVisible': {
              outline: '3px solid var(--ludork-focus)',
              outlineOffset: 3,
            },
          },
        },
      },
      MuiButton: {
        styleOverrides: {
          root: {
            borderRadius: 980,
            transition: reducedMotion ? 'none' : 'background-color 180ms ease, box-shadow 180ms ease, transform 180ms ease',
            '&.MuiButton-contained.MuiButton-colorPrimary': {
              color: '#fff',
              backgroundColor: '#0071e3',
              '&:hover': { backgroundColor: '#0077ed' },
            },
          },
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
      MuiMenu: {
        defaultProps: { transitionDuration: reducedMotion ? 0 : 200 },
        styleOverrides: {
          paper: {
            marginTop: 8,
            padding: 4,
            borderRadius: 16,
            backgroundImage: 'none',
            border: '1px solid var(--ludork-line)',
            boxShadow: 'var(--ludork-shadow)',
          },
          list: { padding: 0 },
        },
      },
      MuiMenuItem: {
        styleOverrides: {
          root: {
            borderRadius: 10,
            minHeight: 40,
            fontSize: '0.875rem',
            '&.Mui-selected': { backgroundColor: 'var(--ludork-selected)' },
          },
        },
      },
    },
  }), [isDark, reducedMotion])

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline enableColorScheme />
      {children}
    </ThemeProvider>
  )
}
