import manifests from 'virtual:ludork-doc-manifest'

export const LUDORK_VERSIONS = Object.keys(manifests)

export function getDocsVersion(): string {
  const routeVersion = window.location.pathname.match(/\/docs\/v(\d+\.\d+)(?:\/|$)/)?.[1]
  return routeVersion ?? new URLSearchParams(window.location.search).get('version') ?? LUDORK_VERSIONS[0]
}

export function isEmbedded(): boolean {
  return new URLSearchParams(window.location.search).get('embedded') === '1'
}
