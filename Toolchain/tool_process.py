from __future__ import annotations

import contextlib
import io
import locale
import os
import pathlib
import signal
import subprocess
import threading
import time
from collections.abc import Callable, Iterator, Mapping, Sequence
from typing import BinaryIO


@contextlib.contextmanager
def cancellation_signals() -> Iterator[None]:
    with _signal_handlers(_interrupt):
        yield


def _interrupt(number: int, frame: object) -> None:
    raise KeyboardInterrupt


@contextlib.contextmanager
def _signal_handlers(handler: Callable, *, only_default: bool = False,
                     only_interrupt: bool = False) -> Iterator[None]:
    signals = (signal.SIGINT, signal.SIGTERM)
    if hasattr(signal, "SIGHUP"):
        signals += (signal.SIGHUP,)
    previous = ({number: signal.getsignal(number) for number in signals}
                if threading.current_thread() is threading.main_thread() else {})
    if only_default:
        previous = {number: original for number, original in previous.items()
                    if original in (signal.SIG_DFL, signal.default_int_handler)}
    if only_interrupt:
        previous = {number: original for number, original in previous.items() if original is _interrupt}
    try:
        for number in previous:
            signal.signal(number, handler)
        yield
    finally:
        for number, original in previous.items():
            signal.signal(number, original)


def _signal_group(pid: int, number: int) -> None:
    deadline = time.monotonic() + 1
    while True:
        try:
            os.killpg(pid, number)
            return
        except ProcessLookupError:
            return
        except PermissionError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


def _stop(process: subprocess.Popen) -> None:
    if os.name == "nt":
        try:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           check=False, timeout=10)
        except (OSError, subprocess.TimeoutExpired):
            pass
        if process.poll() is None:
            process.kill()
    else:
        _signal_group(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
    if os.name != "nt":
        _signal_group(process.pid, signal.SIGKILL)


@_signal_handlers(_interrupt, only_default=True)
def run(command: Sequence[str | pathlib.Path] | str, *, cwd: pathlib.Path | str | None = None,
        env: Mapping[str, str] | None = None, input: bytes | str | None = None,
        stdin=None, stdout=None, stderr=None, capture_output: bool = False,
        text: bool = False, encoding: str | None = None, errors: str | None = None,
        check: bool = False, timeout: float | None = None, creationflags: int = 0,
        cancel_check: Callable[[], None] | None = None,
        on_stdout: Callable[[str | bytes], None] | None = None,
        on_stderr: Callable[[str | bytes], None] | None = None) -> subprocess.CompletedProcess:
    if input is not None:
        if stdin is not None:
            raise ValueError("stdin and input arguments may not both be used.")
        stdin = subprocess.PIPE
    if capture_output:
        if stdout is not None or stderr is not None or on_stdout or on_stderr:
            raise ValueError("capture_output cannot be combined with output redirection or callbacks.")
        stdout = stderr = subprocess.PIPE
    if on_stdout is not None:
        if stdout is not None:
            raise ValueError("stdout and on_stdout may not both be used.")
        stdout = subprocess.PIPE
    if on_stderr is not None:
        if stderr is not None:
            raise ValueError("stderr and on_stderr may not both be used.")
        stderr = subprocess.PIPE
    text_mode = text or encoding is not None or errors is not None
    codec = encoding or locale.getpreferredencoding(False)
    error_mode = errors or "strict"
    if text_mode and input is not None:
        if not isinstance(input, str):
            raise TypeError("Text-mode input must be a string.")
        input = input.replace("\n", os.linesep).encode(codec, error_mode)
    elif isinstance(input, str):
        raise TypeError("Binary-mode input must be bytes.")
    if cancel_check is not None:
        cancel_check()
    arguments = command if isinstance(command, str) else [str(value) for value in command]
    process = None
    output_parts: list[bytes] = []
    error_parts: list[bytes] = []
    failures: list[BaseException] = []
    changed = threading.Event()
    workers: list[threading.Thread] = []
    launch_interrupted = False

    def defer_launch_interrupt(number: int, frame: object) -> None:
        nonlocal launch_interrupted
        launch_interrupted = True

    def read(stream: BinaryIO, parts: list[bytes], callback: Callable | None) -> None:
        try:
            with stream:
                if callback is None:
                    while chunk := stream.read1(65536):
                        parts.append(chunk)
                elif text_mode:
                    with io.TextIOWrapper(stream, encoding=codec, errors=error_mode) as reader:
                        for line in reader:
                            callback(line)
                else:
                    for line in stream:
                        callback(line)
        except BaseException as error:
            failures.append(error)
        finally:
            changed.set()

    def write() -> None:
        try:
            with process.stdin:
                if input is not None:
                    process.stdin.write(input)
        except BrokenPipeError:
            pass
        except OSError as error:
            if not (os.name == "nt" and error.errno == 22):
                failures.append(error)
        except BaseException as error:
            failures.append(error)
        finally:
            changed.set()

    def start(target: Callable, *args) -> None:
        worker = threading.Thread(target=target, args=args)
        workers.append(worker)
        worker.start()

    try:
        with _signal_handlers(defer_launch_interrupt, only_interrupt=True):
            process = subprocess.Popen(
                arguments, cwd=cwd, env=env, stdin=stdin, stdout=stdout, stderr=stderr,
                start_new_session=os.name != "nt",
                creationflags=creationflags | (subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0),
            )
        if launch_interrupted:
            raise KeyboardInterrupt
        deadline = None if timeout is None else time.monotonic() + timeout
        if process.stdout is not None:
            start(read, process.stdout, output_parts, on_stdout)
        if process.stderr is not None:
            start(read, process.stderr, error_parts, on_stderr)
        if process.stdin is not None:
            start(write)
        while True:
            if failures:
                raise failures[0]
            if cancel_check is not None:
                cancel_check()
            if process.poll() is not None and not any(worker.is_alive() for worker in workers):
                if failures:
                    raise failures[0]
                break
            remaining = None if deadline is None else deadline - time.monotonic()
            if remaining is not None and remaining <= 0:
                raise subprocess.TimeoutExpired(arguments, timeout)
            changed.wait(0.2 if remaining is None else min(0.2, remaining))
            changed.clear()
    except BaseException as error:
        if process is None:
            raise
        with _signal_handlers(lambda *_: None):
            _stop(process)
            for worker in workers:
                if worker.ident is not None:
                    worker.join()
        if isinstance(error, subprocess.TimeoutExpired):
            error.output = b"".join(output_parts) if process.stdout is not None else None
            error.stderr = b"".join(error_parts) if process.stderr is not None else None
        raise
    finally:
        if process is not None:
            for stream in (process.stdin, process.stdout, process.stderr):
                if stream is not None:
                    stream.close()

    def captured(parts: list[bytes], stream: BinaryIO | None, callback: Callable | None):
        if stream is None or callback is not None:
            return None
        data = b"".join(parts)
        return data.decode(codec, error_mode).replace("\r\n", "\n").replace("\r", "\n") if text_mode else data

    result = subprocess.CompletedProcess(
        arguments, process.returncode,
        captured(output_parts, process.stdout, on_stdout),
        captured(error_parts, process.stderr, on_stderr),
    )
    if check:
        result.check_returncode()
    return result
