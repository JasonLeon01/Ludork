import { useMemo, type ReactNode } from 'react'
import { createTheme, CssBaseline, ThemeProvider, useMediaQuery } from '@mui/material'
import { MEDIA_QUERIES, THEME_EASE, THEME_FONT_FAMILY, THEME_PALETTE } from '../styles/breakpoints'
import '../styles/tokens.css'
import '../styles/base.css'

export default function Theme({ children }: { children: ReactNode }) {
  const isDark = useMediaQuery(MEDIA_QUERIES.darkColorScheme, { noSsr: true })
  const reducedMotion = useMediaQuery(MEDIA_QUERIES.reducedMotion, { noSsr: true })
  const theme = useMemo(() => {
    const colors = isDark ? THEME_PALETTE.dark : THEME_PALETTE.light
    return createTheme({
      typography: {
        fontFamily: THEME_FONT_FAMILY,
        fontWeightBold: 600,
        button: { textTransform: 'none', fontWeight: 600 },
      },
      palette: {
        mode: isDark ? 'dark' : 'light',
        primary: { main: colors.primary },
        text: {
          primary: colors.textPrimary,
          secondary: colors.textSecondary,
        },
        background: {
          default: colors.backgroundDefault,
          paper: colors.backgroundPaper,
        },
        divider: colors.divider,
        action: {
          hover: colors.actionHover,
          selected: colors.actionSelected,
        },
      },
      shape: { borderRadius: 12 },
      transitions: {
        easing: { easeOut: THEME_EASE },
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
    })
  }, [isDark, reducedMotion])

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline enableColorScheme />
      {children}
    </ThemeProvider>
  )
}
