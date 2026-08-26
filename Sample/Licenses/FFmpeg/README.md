# FFmpeg notice

The optional Ludork video playback component uses FFmpeg 8.1.2 from the
official FFmpeg GitHub mirror at commit
`38b88335f99e76ed89ff3c93f877fdefce736c13`. The project homepage is
https://ffmpeg.org/. Windows and macOS builds use shared FFmpeg libraries.
The C++ Source + FFmpeg template builds static FFmpeg archives for iOS,
HarmonyOS and Android and links them into the application. Ludork does not
provide a Standalone iOS, HarmonyOS or Android packaging path.

The bundled FFmpeg build is configured without GPL, version 3, or nonfree
components and is distributed under the GNU Lesser General Public License
version 2.1 or later. The generated FFmpeg templates include the complete
LGPL 2.1 text as `Licenses/FFmpeg/COPYING.LGPLv2.1.txt`.

Ludork also distributes the complete unmodified FFmpeg source archive. That
archive contains files available under different members of FFmpeg's GPL/LGPL
licence family, although the configured Ludork binary excludes GPL and
version-3-only components. The upstream licensing summary and all four full
licence texts are mirrored as `UPSTREAM-LICENSE.md`, `COPYING.GPLv2.txt`,
`COPYING.GPLv3.txt`, `COPYING.LGPLv2.1.txt`, and `COPYING.LGPLv3.txt` in this
directory and remain present in the distributed source tree.

Only the file protocol, MOV/MP4 demuxer, H.264 and AAC decoders and parsers,
libswscale, and libswresample are enabled. The exact configure and build
scripts are included in `cmake/FFmpeg` for C++ source projects and in
`ThirdPartySource/FFmpeg-Build` for Standalone projects.

The FFmpeg-enabled project template includes the complete corresponding
original FFmpeg source archive in `ThirdPartySource`. The desktop packaging
workflow preserves that directory. The distributed
`msvc-localized-output.patch` changes only MSVC identification so localised
compiler banners are accepted. Preserve this notice, the LGPL text, the source
archive, the patch, and the build scripts when redistributing an FFmpeg-enabled
build.

An iOS, HarmonyOS or Android application that statically links FFmpeg has
additional LGPL requirements. Project templates include the licence notices,
and the mobile packers preserve them when they are present, but they do not
require them as a package-format prerequisite or embed the source archive or
application relinking materials. Before distributing that application, the
distributor must provide the required notices and materials by a compliant
means, including the machine-readable materials that allow recipients to
relink the application with a modified compatible FFmpeg build. The FFmpeg
source archive alone is
not a substitute for any required application object files or other relinking
materials. Review the complete licence and the intended distribution channel
before release.

FFmpeg is a trademark of Fabrice Bellard, originator of the FFmpeg project.
Ludork is not affiliated with or endorsed by the FFmpeg project.
