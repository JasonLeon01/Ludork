import { Box } from '@mui/material'
import chevronLeftSvg from './icons/chevron-left.svg?raw'
import githubSvg from './icons/github.svg?raw'
import homeSvg from './icons/home.svg?raw'
import menuSvg from './icons/menu.svg?raw'

type LudorkIconProps = {
  size?: number
}

function LudorkIcon({ svg, size = 20 }: LudorkIconProps & { svg: string }) {
  return (
    <Box
      component="span"
      aria-hidden
      sx={{
        display: 'inline-flex',
        width: size,
        height: size,
        lineHeight: 0,
        '& svg': { width: '100%', height: '100%', display: 'block' },
      }}
      dangerouslySetInnerHTML={{ __html: svg }}
    />
  )
}

export function GitHubIcon(props: LudorkIconProps) {
  return <LudorkIcon svg={githubSvg} {...props} />
}

export function HomeIcon(props: LudorkIconProps) {
  return <LudorkIcon svg={homeSvg} {...props} />
}

export function ChevronLeftIcon(props: LudorkIconProps) {
  return <LudorkIcon svg={chevronLeftSvg} {...props} />
}

export function MenuIcon(props: LudorkIconProps) {
  return <LudorkIcon svg={menuSvg} {...props} />
}
