import type { LudorkLogoItem } from './LudorkLogoScroller'
import dotnet from './assets/dependencies/dotnet.svg?no-inline'
import avalonia from './assets/dependencies/avalonia.png'
import lua from './assets/dependencies/lua.gif'
import sfml from './assets/dependencies/sfml.svg?no-inline'
import react from './assets/dependencies/react.svg?no-inline'
import mui from './assets/dependencies/mui.svg?no-inline'

export const LUDORK_DEPENDENCIES: readonly LudorkLogoItem[] = [
  { name: '.NET', website: 'https://dotnet.microsoft.com/', icon: dotnet },
  { name: 'Avalonia', website: 'https://avaloniaui.net/', icon: avalonia },
  { name: 'Lua', website: 'https://www.lua.org/', icon: lua },
  { name: 'LuaSF', website: 'https://github.com/JasonLeon01/LuaSF-AutoGenerator' },
  { name: 'SFML', website: 'https://www.sfml-dev.org/', icon: sfml, wide: true },
  { name: 'sol2', website: 'https://github.com/ThePhD/sol2' },
  { name: 'React', website: 'https://react.dev/', icon: react },
  { name: 'MUI', website: 'https://mui.com/', icon: mui },
]
