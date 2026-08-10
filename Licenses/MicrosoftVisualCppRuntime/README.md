# Microsoft Visual C++ Runtime

Windows editor packages include unmodified app-local copies of these Microsoft Visual C++ Runtime files:

- `msvcp140.dll`
- `msvcp140_atomic_wait.dll`
- `vcruntime140.dll`
- `vcruntime140_1.dll`

The packaging script copies them from the `VC\Redist` directory of the Visual Studio instance selected by CMake. If the CMake generator has no Visual Studio instance, it uses the latest installed Visual C++ redistributable component. The exact version is therefore the redistributable version installed in the Visual Studio instance used to produce that Windows package.

The original English **Microsoft Visual C++ Runtime 2015–2022 Software** licence is retained unchanged in `Visual-C-Runtime-2015-2022-License.docx`. It was downloaded from Microsoft's Visual Studio Licence Directory; it is the legal text and has not been translated. Redistribution is also subject to the packager's applicable Visual Studio licence and the Microsoft Visual C++ redistribution documentation. Use the governing official sources:

- https://visualstudio.microsoft.com/license-terms/vs2022-cruntime/
- https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution
- https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files

Before publishing each Windows package, the release owner must manually verify that:

1. the packager is licensed to use the selected Visual Studio installation;
2. every copied DLL is listed as redistributable by the terms and REDIST documentation supplied with that installation;
3. the exact `Microsoft.VCRedistVersion.default.txt` value is recorded in the release record; and
4. the bundled Runtime licence and the official terms above remain applicable and have not introduced conditions that conflict with the planned distribution.
