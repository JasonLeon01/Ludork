/// <reference types="vite/client" />

declare module 'virtual:ludork-doc-filenames' {
  const filenames: Readonly<Record<string, readonly string[]>>
  export default filenames
}

