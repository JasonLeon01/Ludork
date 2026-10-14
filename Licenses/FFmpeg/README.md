# FFmpeg notice

Optional video playback uses [FFmpeg](https://ffmpeg.org/) 8.1.2 from the official GitHub mirror, commit `38b88335f99e76ed89ff3c93f877fdefce736c13`. Windows and macOS use shared libraries; C++ Source + FFmpeg templates link static libraries on iOS, HarmonyOS and Android. Standalone mobile packaging is unavailable.

The configured build excludes GPL, version 3 and nonfree components and is distributed under LGPL 2.1 or later. FFmpeg templates include the complete [LGPL 2.1 text](COPYING.LGPLv2.1.txt).

The complete unmodified source archive also contains files under other FFmpeg GPL/LGPL licences. This directory and the distributed source tree retain the [upstream summary](UPSTREAM-LICENSE.md) and all four texts: [GPL v2](COPYING.GPLv2.txt), [GPL v3](COPYING.GPLv3.txt), [LGPL v2.1](COPYING.LGPLv2.1.txt) and [LGPL v3](COPYING.LGPLv3.txt).

Only the file protocol, MOV/MP4 demuxer, H.264/AAC decoders and parsers, libswscale and libswresample are enabled. Configure/build scripts live in `Engine/cmake/FFmpeg` for C++ Source projects and `ThirdPartySource/FFmpeg-Build` for Standalone projects.

FFmpeg templates include the corresponding original source archive in `ThirdPartySource`, which desktop packaging preserves. The supplied `msvc-localized-output.patch` only enables MSVC identification from localised compiler banners. Redistributors must preserve this notice, the LGPL text, source archive, patch and build scripts.

Static linking on iOS, HarmonyOS or Android adds LGPL obligations. Mobile packers preserve supplied notices but do not require them as package prerequisites or embed source archives or application relinking materials. Distributors must provide the required notices and materials by a compliant means, including machine-readable materials for relinking the application with a compatible modified FFmpeg. FFmpeg sources alone do not replace required application object files or other relinking materials. Review the full licence and intended distribution channel before release.

FFmpeg is a trademark of Fabrice Bellard, the project's originator. Ludork is not affiliated with or endorsed by FFmpeg.
