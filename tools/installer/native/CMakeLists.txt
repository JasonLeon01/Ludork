cmake_minimum_required(VERSION 3.21)
project(LudorkInstallerActions LANGUAGES CXX RC)
if(NOT WIN32)
    message(FATAL_ERROR "Installer custom actions require a Windows toolchain.")
endif()
add_library(LudorkInstallerActions SHARED src/InstallerActions.cpp src/UserDataRemoval.cpp
    resources/UserDataRemoval.rc)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_sources(LudorkInstallerActions PRIVATE src/ExeUninstallActions.def)
endif()
target_compile_features(LudorkInstallerActions PRIVATE cxx_std_20)
target_compile_definitions(LudorkInstallerActions PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_link_libraries(LudorkInstallerActions PRIVATE msi advapi32 shell32 userenv user32)
set_target_properties(LudorkInstallerActions PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
    target_compile_options(LudorkInstallerActions PRIVATE /GR- /EHsc /W4)
elseif(MINGW)
    target_compile_options(LudorkInstallerActions PRIVATE -fno-rtti -fexceptions)
    target_link_options(LudorkInstallerActions PRIVATE -static -static-libgcc -static-libstdc++)
endif()
