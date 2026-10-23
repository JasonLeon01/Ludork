# Third-party notices

The server installs Node.js runtime components through `npm ci --omit=dev` and includes the React components in its built frontend assets. Their licenses are included in the deployable package.

| Component | Version | Source | License notice |
| --- | --- | --- | --- |
| Wasmoon | 1.16.0 | [ceifa/wasmoon](https://github.com/ceifa/wasmoon/tree/87c783a8d0bf65afeda933fa47d8dc996be53bb3) | [MIT](wasmoon-LICENSE.txt) |
| Lua, embedded in Wasmoon's WebAssembly runtime | 5.4.5 | [lua/lua](https://github.com/lua/lua/tree/be908a7d4d8130264ad67c5789169769f824c5d1) | [MIT](lua-LICENSE.txt) |
| ws | 8.22.0 | [websockets/ws](https://github.com/websockets/ws/tree/8.22.0) | [MIT](ws-LICENSE.txt) |
| React | 19.3.0 | [facebook/react](https://github.com/facebook/react) | [MIT](react-LICENSE.txt) |
| React DOM | 19.3.0 | [facebook/react](https://github.com/facebook/react/tree/main/packages/react-dom) | [MIT](react-dom-LICENSE.txt) |
| Scheduler | 0.28.0 | [facebook/react](https://github.com/facebook/react/tree/main/packages/scheduler) | [MIT](scheduler-LICENSE.txt) |

The Wasmoon, ws, React, React DOM and Scheduler notices are reproduced from the corresponding installed npm packages. The Lua notice is reproduced from `lua.h` at the Lua submodule revision recorded by Wasmoon's published 1.16.0 source commit.

Other installed dependencies retain their own package licenses in `node_modules`.
