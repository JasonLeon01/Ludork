import windowsIcon from './assets/platforms/windows.svg?no-inline'
import macosIcon from './assets/platforms/macos.svg?no-inline'
import iosIcon from './assets/platforms/ios.svg?no-inline'
import openharmonyIcon from './assets/platforms/openharmony.svg?no-inline'
import androidIcon from './assets/platforms/android.svg?no-inline'

export type LudorkPlatform = {
  name: string
  icon: string
  website: string
  wide?: boolean
}

const WINDOWS: LudorkPlatform = { name: 'Windows', icon: windowsIcon, website: 'https://www.microsoft.com/windows/' }
const MACOS: LudorkPlatform = { name: 'macOS', icon: macosIcon, website: 'https://www.apple.com/macos/' }

export const LUDORK_EDITOR_PLATFORMS: readonly LudorkPlatform[] = [WINDOWS, MACOS]

export const LUDORK_GAME_PLATFORMS: readonly LudorkPlatform[] = [
  WINDOWS,
  MACOS,
  { name: 'iOS', icon: iosIcon, website: 'https://www.apple.com/ios/' },
  { name: 'OpenHarmony', icon: openharmonyIcon, website: 'https://www.openharmony.cn/', wide: true },
  { name: 'Android', icon: androidIcon, website: 'https://www.android.com/' },
]
