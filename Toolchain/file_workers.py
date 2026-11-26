from __future__ import annotations

import os

from .packaging_constants import PROCESSOR_COUNT_ENVIRONMENT


def process_worker_count() -> int:
    editor_count = os.environ.get(PROCESSOR_COUNT_ENVIRONMENT)
    if editor_count:
        return max(1, int(editor_count))
    processors = os.cpu_count() or 1
    if os.name == "nt":
        import ctypes

        kernel = ctypes.windll.kernel32
        process_mask = ctypes.c_size_t()
        system_mask = ctypes.c_size_t()
        current_process = ctypes.c_void_p(-1)
        if kernel.GetProcessAffinityMask(current_process, ctypes.byref(process_mask), ctypes.byref(system_mask)) and process_mask.value:
            processors = process_mask.value.bit_count()
        # CPU rate control in a Windows job can further restrict the process.
        rate = (ctypes.c_uint32 * 2)()
        if kernel.QueryInformationJobObject(None, 15, ctypes.byref(rate), ctypes.sizeof(rate), None):
            flags, limits = rate
            cap = limits if flags & 4 else (limits >> 16) if flags & 16 else 0
            if flags & 1 and cap:
                processors = min(processors, ((os.cpu_count() or 1) * cap + 9999) // 10000)
    elif hasattr(os, "sched_getaffinity"):
        processors = len(os.sched_getaffinity(0))
    return max(1, processors)
