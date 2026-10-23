# Third-party notices

The server installs these runtime components through `npm ci`. Their licenses are included with source archives as well as in their installed packages.

| Component | Version | Source | License notice |
| --- | --- | --- | --- |
| Wasmoon | 1.16.0 | [ceifa/wasmoon](https://github.com/ceifa/wasmoon/tree/87c783a8d0bf65afeda933fa47d8dc996be53bb3) | [MIT](wasmoon-LICENSE.txt) |
| Lua, embedded in Wasmoon's WebAssembly runtime | 5.4.5 | [lua/lua](https://github.com/lua/lua/tree/be908a7d4d8130264ad67c5789169769f824c5d1) | [MIT](lua-LICENSE.txt) |
| ws | 8.22.0 | [websockets/ws](https://github.com/websockets/ws/tree/8.22.0) | [MIT](ws-LICENSE.txt) |

The Wasmoon and ws notices are reproduced from the corresponding installed npm packages. The Lua notice is reproduced from `lua.h` at the Lua submodule revision recorded by Wasmoon's published 1.16.0 source commit.

Other installed dependencies retain their own package licenses in `node_modules`; this directory records the networking and Lua runtime components introduced by this server architecture.
