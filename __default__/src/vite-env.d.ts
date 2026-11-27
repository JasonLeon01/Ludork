/// <reference types="vite/client" />

declare module 'virtual:ludork-versions' {
  const versions: readonly string[]
  export default versions
}

declare module 'highlight.js/lib/core' {
  import type { HLJSApi } from 'highlight.js'
  const hljs: HLJSApi
  export default hljs
}
