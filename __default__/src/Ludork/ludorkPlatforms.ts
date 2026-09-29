import windowsIcon from './assets/platforms/windows.svg?no-inline'
import macosIcon from './assets/platforms/macos.svg?no-inline'
import iosIcon from './assets/platforms/ios.svg?no-inline'
import openharmonyIcon from './assets/platforms/openharmony.svg?no-inline'
import androidIcon from './assets/platforms/android.svg?no-inline'

export type LudorkPlatform = {
  name: string
  icon: string
  wide?: boolean
}

const WINDOWS: LudorkPlatform = { name: 'Windows', icon: windowsIcon }
const MACOS: LudorkPlatform = { name: 'macOS', icon: macosIcon }

export const LUDORK_EDITOR_PLATFORMS: readonly LudorkPlatform[] = [WINDOWS, MACOS]

export const LUDORK_GAME_PLATFORMS: readonly LudorkPlatform[] = [
  WINDOWS,
  MACOS,
  { name: 'iOS', icon: iosIcon },
  { name: 'OpenHarmony', icon: openharmonyIcon, wide: true },
  { name: 'Android', icon: androidIcon },
]
