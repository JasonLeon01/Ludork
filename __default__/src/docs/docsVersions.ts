import versions from 'virtual:ludork-versions'

export const LUDORK_VERSIONS: readonly string[] = versions

export function getDocsVersion(): string {
  const route = window.location.pathname.match(/\/docs\/(trunk|v\d+\.\d+)(?:\/|$)/)?.[1]
  const routeVersion = route?.startsWith('v') ? route.slice(1) : route
  return routeVersion ?? new URLSearchParams(window.location.search).get('version') ?? LUDORK_VERSIONS[0]
}

export function isEmbedded(): boolean {
  return new URLSearchParams(window.location.search).get('embedded') === '1'
}
