from __future__ import annotations

import ctypes
from ctypes import wintypes
import os
import pathlib


class _UniqueProcess(ctypes.Structure):
    _fields_ = [
        ("pid", wintypes.DWORD),
        ("started", wintypes.FILETIME),
    ]


class _ProcessInfo(ctypes.Structure):
    _fields_ = [
        ("process", _UniqueProcess),
        ("application", wintypes.WCHAR * 256),
        ("service", wintypes.WCHAR * 64),
        ("application_type", ctypes.c_int),
        ("status", wintypes.ULONG),
        ("session", wintypes.DWORD),
        ("restartable", wintypes.BOOL),
    ]


def describe_locks(path: pathlib.Path) -> str:
    if os.name != "nt":
        return ""
    try:
        if not path.is_file():
            return ""
        return _query_locks(str(path.resolve()))
    except (OSError, ValueError, ctypes.ArgumentError):
        return ""


def _query_locks(path: str) -> str:
    manager = ctypes.WinDLL("Rstrtmgr.dll")
    manager.RmStartSession.argtypes = [
        ctypes.POINTER(wintypes.DWORD), wintypes.DWORD, wintypes.LPWSTR,
    ]
    manager.RmStartSession.restype = wintypes.DWORD
    manager.RmRegisterResources.argtypes = [
        wintypes.DWORD, wintypes.UINT, ctypes.POINTER(wintypes.LPCWSTR),
        wintypes.UINT, ctypes.POINTER(_UniqueProcess),
        wintypes.UINT, ctypes.POINTER(wintypes.LPCWSTR),
    ]
    manager.RmRegisterResources.restype = wintypes.DWORD
    manager.RmGetList.argtypes = [
        wintypes.DWORD, ctypes.POINTER(wintypes.UINT),
        ctypes.POINTER(wintypes.UINT), ctypes.POINTER(_ProcessInfo),
        ctypes.POINTER(wintypes.DWORD),
    ]
    manager.RmGetList.restype = wintypes.DWORD
    manager.RmEndSession.argtypes = [wintypes.DWORD]
    manager.RmEndSession.restype = wintypes.DWORD

    session = wintypes.DWORD()
    key = ctypes.create_unicode_buffer(33)
    if manager.RmStartSession(ctypes.byref(session), 0, key) != 0:
        return ""
    try:
        files = (wintypes.LPCWSTR * 1)(path)
        if manager.RmRegisterResources(session, 1, files, 0, None, 0, None) != 0:
            return ""
        capacity = 0
        for _ in range(5):
            needed = wintypes.UINT()
            count = wintypes.UINT(capacity)
            reasons = wintypes.DWORD()
            processes = (_ProcessInfo * capacity)() if capacity else None
            result = manager.RmGetList(
                session, ctypes.byref(needed), ctypes.byref(count),
                processes, ctypes.byref(reasons),
            )
            if result == 0:
                if processes is None or count.value > capacity:
                    return ""
                names = {}
                for info in processes[:count.value]:
                    pid = info.process.pid
                    if pid:
                        name = " ".join((info.application or info.service).split())
                        names[pid] = f"{name} (PID {pid})" if name else f"PID {pid}"
                return "; ".join(names[pid] for pid in sorted(names))
            if result != 234 or not 0 < needed.value <= 4096:
                return ""
            capacity = needed.value
        return ""
    finally:
        manager.RmEndSession(session)
