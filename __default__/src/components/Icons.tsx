import type { ReactNode } from 'react'

type IconProps = {
  size?: number
}

function IconFrame({ size = 20, viewBox, fill = 'none', stroke, strokeWidth, children }: {
  size?: number
  viewBox: string
  fill?: string
  stroke?: string
  strokeWidth?: number
  children: ReactNode
}) {
  return (
    <span
      aria-hidden
      style={{
        display: 'inline-flex',
        width: size,
        height: size,
        lineHeight: 0,
        flexShrink: 0,
      }}
    >
      <svg
        xmlns="http://www.w3.org/2000/svg"
        width={size}
        height={size}
        viewBox={viewBox}
        fill={fill}
        stroke={stroke}
        strokeWidth={strokeWidth}
        strokeLinecap={stroke ? 'round' : undefined}
        strokeLinejoin={stroke ? 'round' : undefined}
        style={{ width: '100%', height: '100%', display: 'block' }}
      >
        {children}
      </svg>
    </span>
  )
}

export function GitHubIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 19 19" fill="currentColor">
      <path
        fillRule="evenodd"
        clipRule="evenodd"
        d="M9.356 1.85C5.05 1.85 1.57 5.356 1.57 9.694a7.84 7.84 0 0 0 5.324 7.44c.387.079.528-.168.528-.376 0-.182-.013-.805-.013-1.454-2.165.467-2.616-.935-2.616-.935-.349-.91-.864-1.143-.864-1.143-.71-.48.051-.48.051-.48.787.051 1.2.805 1.2.805.695 1.194 1.817.857 2.268.649.064-.507.27-.857.49-1.052-1.728-.182-3.545-.857-3.545-3.87 0-.857.31-1.558.8-2.104-.078-.195-.349-1 .077-2.078 0 0 .657-.208 2.14.805a7.5 7.5 0 0 1 1.946-.26c.657 0 1.328.092 1.946.26 1.483-1.013 2.14-.805 2.14-.805.426 1.078.155 1.883.078 2.078.502.546.799 1.247.799 2.104 0 3.013-1.818 3.675-3.558 3.87.284.247.528.714.528 1.454 0 1.052-.012 1.896-.012 2.156 0 .208.142.455.528.377a7.84 7.84 0 0 0 5.324-7.441c.013-4.338-3.48-7.844-7.773-7.844"
      />
    </IconFrame>
  )
}

export function GlobeIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.75}>
      <circle cx="12" cy="12" r="9" />
      <ellipse cx="12" cy="12" rx="4" ry="9" />
      <path d="M3 12h18" />
    </IconFrame>
  )
}

export function ChevronLeftIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
      <path d="M15 18l-6-6 6-6" />
    </IconFrame>
  )
}

export function MenuIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
      <line x1="3" y1="6" x2="21" y2="6" />
      <line x1="3" y1="12" x2="21" y2="12" />
      <line x1="3" y1="18" x2="21" y2="18" />
    </IconFrame>
  )
}

export function SunIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.75}>
      <circle cx="12" cy="12" r="4" />
      <path d="M12 2.5v2M12 19.5v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2.5 12h2M19.5 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41" />
    </IconFrame>
  )
}

export function MoonIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.75}>
      <path d="M20.5 14.5A8.5 8.5 0 1 1 9.5 3.5a6.5 6.5 0 0 0 11 11z" />
    </IconFrame>
  )
}

export function MonitorIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.75}>
      <rect x="3" y="4" width="18" height="12" rx="2" />
      <path d="M8 20h8M12 16v4" />
    </IconFrame>
  )
}

export function SearchIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.85}>
      <circle cx="11" cy="11" r="7" />
      <path d="M20 20l-3.8-3.8" />
    </IconFrame>
  )
}

export function CopyIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.75}>
      <rect x="9" y="9" width="11" height="11" rx="2" />
      <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" />
    </IconFrame>
  )
}

export function CheckIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
      <path d="M20 6L9 17l-5-5" />
    </IconFrame>
  )
}

export function LinkIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.85}>
      <path d="M10 13a5 5 0 0 0 7.07 0l2.83-2.83a5 5 0 0 0-7.07-7.07L11.4 4.5" />
      <path d="M14 11a5 5 0 0 0-7.07 0L4.1 13.83a5 5 0 1 0 7.07 7.07l1.42-1.41" />
    </IconFrame>
  )
}

export function CloseIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
      <path d="M18 6L6 18M6 6l12 12" />
    </IconFrame>
  )
}

export function ChevronRightIcon({ size }: IconProps) {
  return (
    <IconFrame size={size} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
      <path d="M9 18l6-6-6-6" />
    </IconFrame>
  )
}
