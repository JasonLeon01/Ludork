# MinGW runtime distribution

Windows x64 games built with MinGW-w64 GCC using POSIX threads and SEH exception handling carry the compiler's `libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll` alongside the native runtime. Their versions follow the configured compiler distribution; Ludork does not replace them with runtime DLLs from a different toolchain.

`libstdc++` and `libgcc` use GNU GPL version 3 or later with the GCC Runtime Library Exception version 3.1. The complete texts are [GCC-COPYING3.txt](GCC-COPYING3.txt) and [GCC-RUNTIME-EXCEPTION.txt](GCC-RUNTIME-EXCEPTION.txt), copied unchanged from the [GCC 16.2.0 source tree](https://github.com/gcc-mirror/gcc/tree/releases/gcc-16.2.0). The exception permits eligible independently licensed application code; redistribution of the runtime DLLs themselves remains subject to the GPL's requirements, including the applicable corresponding-source provision in section 6. See the [GNU exception FAQ](https://www.gnu.org/licenses/gcc-exception-3.1-faq.html).

When distributing a game with these DLLs, provide the corresponding source using a method permitted by that licence. Obtain the source, patches and build materials matching the actual compiler distribution from its provider, and retain its notices. The [official GCC source releases](https://gcc.gnu.org/pub/gcc/releases/) identify upstream versions; an upstream URL alone does not establish that a third-party binary's complete corresponding source has been supplied.

`libwinpthread-1.dll` comes from MinGW-w64 winpthreads under MIT and incorporated BSD 3-Clause terms. [winpthreads-COPYING.txt](winpthreads-COPYING.txt) is copied unchanged from [MinGW-w64 v14.0.0](https://github.com/mingw-w64/mingw-w64/blob/v14.0.0/mingw-w64-libraries/winpthreads/COPYING). Preserve the notices supplied with the selected MinGW distribution.

The MinGW game runtime also carries the architecture-matched `WebView2Loader.dll` from Microsoft.Web.WebView2 1.0.4191.47. Its unchanged SDK [licence](WebView2-LICENSE.txt) and [incorporated notices](WebView2-NOTICE.txt) accompany it. The loader locates the installed Evergreen WebView2 Runtime; the game package does not contain the browser engine.

## MinGW 运行库分发

使用 POSIX 线程及 SEH 异常处理的 MinGW-w64 GCC 构建的 Windows x64 游戏，会在原生运行时旁随附编译器的 `libstdc++-6.dll`、`libgcc_s_seh-1.dll` 与 `libwinpthread-1.dll`。版本取决于配置的编译器分发包；Ludork 不会用另一套工具链的 DLL 替换它们。

`libstdc++` 与 `libgcc` 适用 GNU GPL 第 3 版或更高版本，并附带 GCC Runtime Library Exception 3.1。完整正文见 [GCC-COPYING3.txt](GCC-COPYING3.txt) 和 [GCC-RUNTIME-EXCEPTION.txt](GCC-RUNTIME-EXCEPTION.txt)，均从 [GCC 16.2.0 源码树](https://github.com/gcc-mirror/gcc/tree/releases/gcc-16.2.0)原样复制。该例外允许符合条件的应用代码使用独立许可证；运行库 DLL 本身的再分发仍须满足 GPL 要求，包括第 6 节中适用的对应源码提供方式，详见 [GNU 例外条款 FAQ](https://www.gnu.org/licenses/gcc-exception-3.1-faq.html)。

随游戏分发这些 DLL 时，按许可证允许的方式提供对应源码。从实际编译器分发方获取匹配的源码、补丁及构建材料，并保留其声明。[GCC 官方源码发布目录](https://gcc.gnu.org/pub/gcc/releases/)可用于识别上游版本；单独提供上游网址不代表已经提供第三方二进制的完整对应源码。

`libwinpthread-1.dll` 来自 MinGW-w64 的 winpthreads，适用 MIT 与所含 BSD 3-Clause 条款。[winpthreads-COPYING.txt](winpthreads-COPYING.txt) 从 [MinGW-w64 v14.0.0](https://github.com/mingw-w64/mingw-w64/blob/v14.0.0/mingw-w64-libraries/winpthreads/COPYING) 原样复制；同时保留所选 MinGW 分发包提供的声明。

MinGW 游戏运行时还会随附 Microsoft.Web.WebView2 1.0.4191.47 中架构匹配的 `WebView2Loader.dll`，并保留 SDK 原始[许可证](WebView2-LICENSE.txt)和[第三方声明](WebView2-NOTICE.txt)。Loader 查找系统安装的 Evergreen WebView2 Runtime；游戏包不包含浏览器内核。
