/// <reference types="vite/client" />

declare module 'virtual:ludork-doc-manifest' {
  const manifests: Record<string, Record<import('./Ludork/ludorkLanguages').LanguageKey, import('./Ludork/ludorkDocsManifest').DocsManifest>>
  export default manifests
}
