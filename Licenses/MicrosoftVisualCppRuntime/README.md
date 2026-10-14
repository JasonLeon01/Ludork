# Microsoft Visual C++ Runtime

Windows editor packages include unmodified app-local copies of `msvcp140.dll`, `msvcp140_atomic_wait.dll`, `vcruntime140.dll` and `vcruntime140_1.dll`.

Packaging copies them from `VC\Redist` in the Visual Studio instance selected by CMake, or the latest installed Visual C++ redistributable component when no instance is selected. Their version comes from that installation.

The original English [Microsoft Visual C++ Runtime 2015–2022 Software licence](Visual-C-Runtime-2015-2022-License.docx), downloaded from Microsoft's Visual Studio Licence Directory, is retained unchanged. Redistribution is also subject to the packager's Visual Studio licence and Microsoft's redistribution terms:

- [Runtime licence](https://visualstudio.microsoft.com/license-terms/vs2022-cruntime/)
- [Visual Studio 2022 redistribution](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution)
- [Redistributing Visual C++ files](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files)

Before publishing each Windows package, the release owner must manually verify:

1. The packager is licensed for the selected Visual Studio installation.
2. Every copied DLL is redistributable under that installation's terms and REDIST documentation.
3. The release record includes the exact `Microsoft.VCRedistVersion.default.txt` value.
4. The bundled Runtime licence and official terms remain applicable and allow the planned distribution.
