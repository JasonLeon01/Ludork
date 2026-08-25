# Ludork Sample 1.0.0 Licences and Third-Party Notices

Ludork itself is licensed under the Zlib License in [`LICENSE.md`](LICENSE.md). That licence applies to Ludork software; it does not replace the licences of the components and assets listed below.

This notice covers the Sample project, the project templates shipped with Ludork 1.0.0, and the unchanged Ludork game-runtime and bundled-asset materials that those templates carry into a generated game package. Project-specific code and assets require their own notices. Complete game-runtime licence texts are listed in the [`game-runtime licence index`](Licenses/README.md), while bundled-asset texts remain beside their assets. C++ Source templates also retain notices embedded in their third-party source trees. Editor, managed-runtime, preview-host, and build-tool dependencies are outside this notice and remain documented with the Ludork editor distribution.

## Native game runtime

| Component | Version or revision | Purpose and distribution | Licence | Complete text | Official source |
| --- | --- | --- | --- | --- | --- |
| LuaSF | v3.1.0.5-ME-OH | SFML bindings in the Sample and all project templates | MIT | `Licenses/LuaSF/LICENSE.txt` | [JasonLeon01/LuaSF-AutoGenerator](https://github.com/JasonLeon01/LuaSF-AutoGenerator) |
| Lua | 5.5.0 | Game scripting runtime and `luac` | MIT | `Licenses/Lua/LICENSE.txt` | [Lua.org](https://www.lua.org/) |
| SFML | 3.1.0, SFML-ME branch 310ME-OH | Graphics, windowing, audio, system, and network runtime | Zlib/libpng | `Licenses/SFML/LICENSE.txt` | [JasonLeon01/SFML-ME `310ME-OH`](https://github.com/JasonLeon01/SFML-ME/tree/310ME-OH) |
| sol2 | 3.2.3 in the distributed LuaSF source | C++/Lua binding headers | MIT | `Licenses/sol2/LICENSE.txt` | [ThePhD/sol2](https://github.com/ThePhD/sol2) |
| lua-cjson | Source release 2.1.0.19; the upstream runtime constant reports 2.1.0.11 | JSON module linked into the game runtime | MIT | `Licenses/lua-cjson/LICENSE.txt` | [openresty/lua-cjson](https://github.com/openresty/lua-cjson) |
| zlib | 1.3.1 | Compression support in the game runtime | Zlib | `Licenses/zlib/LICENSE.txt` | [madler/zlib](https://github.com/madler/zlib) |
| FreeType | 2.14.3 | Statically linked SFML font dependency | FreeType License or GPLv2; Ludork uses the FreeType License option | `Licenses/NativeDependencies/FreeType-LICENSE.txt`, `Licenses/NativeDependencies/FreeType-FTL.txt` | [freetype/freetype](https://gitlab.freedesktop.org/freetype/freetype) |
| HarfBuzz | 14.1.0 | Statically linked SFML text-shaping dependency | Old MIT | `Licenses/NativeDependencies/HarfBuzz-COPYING.txt` | [harfbuzz/harfbuzz](https://github.com/harfbuzz/harfbuzz) |
| SheenBidi | 3.0.0 | Statically linked SFML bidirectional-text dependency | Apache License 2.0 | `Licenses/NativeDependencies/SheenBidi-LICENSE.txt` | [Tehreer/SheenBidi](https://github.com/Tehreer/SheenBidi) |
| Ogg | 1.3.6 | Statically linked SFML audio container dependency | BSD 3-Clause | `Licenses/NativeDependencies/Ogg-COPYING.txt` | [xiph/ogg](https://github.com/xiph/ogg) |
| Vorbis | 1.3.7 | Statically linked SFML audio codec dependency | BSD 3-Clause | `Licenses/NativeDependencies/Vorbis-COPYING.txt` | [xiph/vorbis](https://github.com/xiph/vorbis) |
| FLAC | 1.5.0 | Statically linked SFML audio codec dependency | BSD 3-Clause | `Licenses/NativeDependencies/FLAC-COPYING.Xiph.txt` | [xiph/flac](https://github.com/xiph/flac) |
| Mbed TLS | 3.6.5 | Statically linked SFML network cryptography dependency | Apache License 2.0 or GPLv2; Ludork uses the Apache option | `Licenses/NativeDependencies/MbedTLS-LICENSE.txt` | [Mbed-TLS/mbedtls](https://github.com/Mbed-TLS/mbedtls) |
| libssh2 | commit `704299e997bf518375dc9222670c57b800ac59e6` (1.11.2 development line) | Statically linked SFML SFTP dependency | BSD 3-Clause | `Licenses/NativeDependencies/libssh2-COPYING.txt` | [libssh2/libssh2](https://github.com/libssh2/libssh2) |
| SFML bundled source dependencies | Revisions bundled with SFML-ME 310ME-OH | OpenGL loading, image, audio, Unicode, Vulkan, DirectInput compatibility, and Windows polling support in SFML source templates | MIT, MIT-0, BSD, Apache 2.0, CC0 1.0, LGPL 2.1 or later, or public-domain alternatives as identified by SFML and the source headers | `Licenses/NativeDependencies/SFML-THIRD-PARTY.md`, `Licenses/NativeDependencies/Glad-CC0-1.0.txt`, `Licenses/NativeDependencies/Wine-DInput-LGPLv2.1.txt`; full notices also remain in the corresponding source headers | [SFML-ME dependency list](https://github.com/JasonLeon01/SFML-ME/blob/310ME-OH/readme.md#external-libraries-used-by-sfml) |

## Video runtime and bundled assets

| Component or asset | Version | Distribution | Terms | Complete text or notice | Official source |
| --- | --- | --- | --- | --- | --- |
| FFmpeg | 8.1.2 | The Sample and FFmpeg-enabled templates; trimmed shared libraries on Windows/macOS, static libraries for iOS, HarmonyOS and Android C++ Source builds, complete source archive, patch, and build configuration | LGPL 2.1 or later for Ludork's configured build; the full source archive also retains FFmpeg's GPL/LGPL licence family, and a distributor of a statically linked iOS, HarmonyOS or Android application must supply the applicable relinking materials | `Licenses/FFmpeg/README.md`, `Licenses/FFmpeg/UPSTREAM-LICENSE.md`, `Licenses/FFmpeg/COPYING.LGPLv2.1.txt`, `Licenses/FFmpeg/COPYING.LGPLv3.txt`, `Licenses/FFmpeg/COPYING.GPLv2.txt`, `Licenses/FFmpeg/COPYING.GPLv3.txt` | [FFmpeg](https://ffmpeg.org/) |
| HarmonyOS Sans SC | Font copyright 2021 Huawei Device Co., Ltd. | `Assets/Fonts/HarmonyOS_SansSC_Medium.ttf` in the Sample and project templates | HarmonyOS Sans Fonts License Agreement | `Assets/Fonts/LICENSE.txt` | [HarmonyOS design resources](https://developer.huawei.com/consumer/en/design/resource/) |
| “To Walk the Unseen Path” | Sample asset | MP3 music currently present in the Sample | Suno Free/Basic terms restrict use to personal, non-commercial purposes; no verified raw-template redistribution grant | `Assets/Musics/LICENSE.md` | [Suno Terms of Service](https://suno.com/terms/) |

The Sample music restriction applies only to that music asset; Ludork software remains available under the Zlib License. The current Suno terms do not establish permission to redistribute the raw track in an editor or project template, even for a non-commercial release. Remove or replace the track, or obtain express redistribution rights, before publishing a package that contains it.

## Source and trademarks

Project names and trade marks belong to their respective owners. Source URLs and upstream attribution are recorded in this notice and in the corresponding licence files. Ludork is not affiliated with or endorsed by those upstream projects.
