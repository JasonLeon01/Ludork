from __future__ import annotations

import contextlib
import os
import pathlib
import signal
import subprocess
import threading
from collections.abc import Iterator, Mapping, Sequence

from .pack_error import PackError


def _stop(process: subprocess.Popen[bytes]) -> None:
    interrupted = False

    def defer_interrupt(number: int, frame: object) -> None:
        nonlocal interrupted
        interrupted = True

    signals = (signal.SIGINT, signal.SIGTERM)
    if hasattr(signal, "SIGHUP"):
        signals += (signal.SIGHUP,)
    previous = (
        {number: signal.getsignal(number) for number in signals}
        if threading.current_thread() is threading.main_thread()
        else {}
    )
    try:
        for number in previous:
            signal.signal(number, defer_interrupt)
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        else:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        if os.name != "nt":
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)
    if interrupted:
        raise KeyboardInterrupt


def run(command: Sequence[str | pathlib.Path] | str, *, cwd: pathlib.Path | None = None,
        env: Mapping[str, str] | None = None, input: bytes | None = None,
        capture: bool = False, quiet: bool = False, check: bool = True,
        timeout: float | None = None) -> subprocess.CompletedProcess[bytes]:
    arguments = command if isinstance(command, str) else [str(value) for value in command]
    with subprocess.Popen(
        arguments, cwd=cwd, env=env,
        stdin=subprocess.PIPE if input is not None else None,
        stdout=subprocess.PIPE if capture else subprocess.DEVNULL if quiet else None,
        stderr=subprocess.PIPE if capture else None,
        start_new_session=os.name != "nt",
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
    ) as process:
        try:
            output, error = process.communicate(input, timeout=timeout)
        except BaseException:
            _stop(process)
            raise
    result = subprocess.CompletedProcess(arguments, process.returncode, output or b"", error or b"")
    if check and result.returncode:
        detail = (result.stderr or result.stdout).decode("utf-8", errors="replace").strip()
        program = "Command" if isinstance(arguments, str) else arguments[0]
        raise PackError(f"{program} failed with exit code {result.returncode}."
                        + (f"\n{detail}" if detail else ""),
                        result.returncode if result.returncode > 0 else 130)
    return result


@contextlib.contextmanager
def cancellation_signals() -> Iterator[None]:
    def interrupt(number: int, frame: object) -> None:
        raise KeyboardInterrupt

    signals = (signal.SIGINT, signal.SIGTERM)
    if hasattr(signal, "SIGHUP"):
        signals += (signal.SIGHUP,)
    previous = {number: signal.getsignal(number) for number in signals}
    try:
        for number in signals:
            signal.signal(number, interrupt)
        yield
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)
