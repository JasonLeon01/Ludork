from __future__ import annotations

import ctypes
import os
import pathlib
import subprocess
from ctypes import wintypes


def windowsUserProfile(expectedSid: str | None) -> pathlib.Path:
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    security = ctypes.WinDLL("advapi32", use_last_error=True)
    profiles = ctypes.WinDLL("userenv", use_last_error=True)
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    security.OpenProcessToken.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)]
    security.OpenProcessToken.restype = wintypes.BOOL
    security.GetTokenInformation.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                             wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    security.GetTokenInformation.restype = wintypes.BOOL
    security.ConvertSidToStringSidW.argtypes = [ctypes.c_void_p, ctypes.POINTER(wintypes.LPWSTR)]
    security.ConvertSidToStringSidW.restype = wintypes.BOOL
    profiles.GetUserProfileDirectoryW.argtypes = [wintypes.HANDLE, wintypes.LPWSTR,
                                                 ctypes.POINTER(wintypes.DWORD)]
    profiles.GetUserProfileDirectoryW.restype = wintypes.BOOL
    token = wintypes.HANDLE()
    if not security.OpenProcessToken(kernel.GetCurrentProcess(), 0x0008, ctypes.byref(token)):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        size = wintypes.DWORD()
        security.GetTokenInformation(token, 1, None, 0, ctypes.byref(size))
        if size.value == 0:
            raise ctypes.WinError(ctypes.get_last_error())
        information = ctypes.create_string_buffer(size.value)
        if not security.GetTokenInformation(token, 1, information, size, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        sidPointer = ctypes.cast(information, ctypes.POINTER(ctypes.c_void_p))[0]
        sidText = wintypes.LPWSTR()
        if not security.ConvertSidToStringSidW(sidPointer, ctypes.byref(sidText)):
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            sid = sidText.value
        finally:
            kernel.LocalFree(ctypes.cast(sidText, ctypes.c_void_p))
        if sid is None or sid in ("S-1-5-18", "S-1-5-19", "S-1-5-20") or sid.startswith(("S-1-5-80-", "S-1-5-82-")):
            raise RuntimeError("Official plug-ins require a user account. Use INSTALL_OFFICIAL_PLUGINS=0 for a service-account installation.")
        if expectedSid is not None and sid.casefold() != expectedSid.casefold():
            raise RuntimeError("The plug-in installer account does not match the account that started setup.")
        length = wintypes.DWORD()
        profiles.GetUserProfileDirectoryW(token, None, ctypes.byref(length))
        if length.value == 0:
            raise ctypes.WinError(ctypes.get_last_error())
        profile = ctypes.create_unicode_buffer(length.value)
        if not profiles.GetUserProfileDirectoryW(token, profile, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        return pathlib.Path(profile.value)
    finally:
        kernel.CloseHandle(token)


def currentUserProfile(expectedSid: str | None = None) -> pathlib.Path:
    if os.name == "nt":
        profile = windowsUserProfile(expectedSid)
    else:
        if expectedSid is not None:
            raise RuntimeError("A Windows user SID cannot be checked on this platform.")
        import pwd

        if os.getuid() == 0:
            raise RuntimeError("Run the official plug-in installer as your user account, without sudo.")
        profile = pathlib.Path(pwd.getpwuid(os.getuid()).pw_dir)
    if not profile.is_absolute() or not profile.is_dir():
        raise RuntimeError("The current account has no available user home directory.")
    return profile


def requireWindowsEditorClosed() -> None:
    class ProcessEntry(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD),
            ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD),
            ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD),
            ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD),
            ("szExeFile", wintypes.WCHAR * 260),
        ]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    for name in ("Process32FirstW", "Process32NextW"):
        function = getattr(kernel, name)
        function.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
        function.restype = wintypes.BOOL
    snapshot = kernel.CreateToolhelp32Snapshot(0x00000002, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = ProcessEntry()
        entry.dwSize = ctypes.sizeof(entry)
        found = kernel.Process32FirstW(snapshot, ctypes.byref(entry))
        while found:
            if entry.szExeFile.casefold() == "ludork.exe":
                raise RuntimeError("Close Ludork before replacing the official plug-ins.")
            found = kernel.Process32NextW(snapshot, ctypes.byref(entry))
        if ctypes.get_last_error() != 18:
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        kernel.CloseHandle(snapshot)


def requireEditorClosed() -> None:
    if os.name == "nt":
        requireWindowsEditorClosed()
        return
    result = subprocess.run(["/usr/bin/pgrep", "-x", "Ludork"], capture_output=True, text=True)
    if result.returncode == 0:
        raise RuntimeError("Close Ludork before replacing the official plug-ins.")
    if result.returncode != 1:
        raise RuntimeError("Could not check whether Ludork is running: " + result.stderr.strip())
