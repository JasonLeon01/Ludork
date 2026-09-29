import filenames from 'virtual:ludork-doc-filenames'

export const LUDORK_VERSIONS = Object.keys(filenames)

export function getDocsVersion(): string {
  const routeVersion = window.location.pathname.match(/\/docs\/v(\d+\.\d+)(?:\/|$)/)?.[1]
  return routeVersion ?? new URLSearchParams(window.location.search).get('version') ?? LUDORK_VERSIONS[0]
}

export function isEmbedded(): boolean {
  return new URLSearchParams(window.location.search).get('embedded') === '1'
}
