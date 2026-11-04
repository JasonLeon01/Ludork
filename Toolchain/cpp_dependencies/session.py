from __future__ import annotations

import contextlib
import hashlib
import os
import pathlib
import signal
import subprocess
import tempfile
import threading
from collections.abc import Iterator
from concurrent.futures import CancelledError


class PreparationSession:
    def __init__(self) -> None:
        self.cancelled = threading.Event()

    def check(self) -> None:
        if self.cancelled.is_set():
            raise CancelledError("Dependency preparation was cancelled.")

    def run(self, command: list[str] | str, *, cwd: pathlib.Path | None = None,
            environment: dict[str, str] | None = None) -> None:
        self.check()
        with subprocess.Popen(command, cwd=cwd, env=environment, stdin=subprocess.DEVNULL,
                              start_new_session=os.name != "nt",
                              creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0) as process:
            try:
                while True:
                    self.check()
                    try:
                        code = process.wait(timeout=0.2)
                        break
                    except subprocess.TimeoutExpired:
                        pass
                if code:
                    raise RuntimeError(f"Command failed with exit code {code}: {command[0] if isinstance(command, list) else command}")
            except BaseException:
                self._stop(process)
                raise

    @staticmethod
    def _stop(process: subprocess.Popen) -> None:
        if process.poll() is not None:
            return
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        else:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            if os.name != "nt":
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            else:
                process.kill()
            process.wait()

    @contextlib.contextmanager
    def signals(self) -> Iterator[None]:
        previous: dict[int, object] = {}
        if threading.current_thread() is threading.main_thread():
            for name in ("SIGINT", "SIGTERM", "SIGHUP"):
                number = getattr(signal, name, None)
                if number is not None:
                    previous[number] = signal.signal(number, lambda *_: self.cancelled.set())
        try:
            yield
        finally:
            for number, handler in previous.items():
                signal.signal(number, handler)


@contextlib.contextmanager
def project_lock(project: pathlib.Path) -> Iterator[None]:
    identity = hashlib.sha256(os.path.normcase(str(project)).encode("utf-8")).hexdigest()
    path = pathlib.Path(tempfile.gettempdir()) / f"ludork-dependencies-{identity}.lock"
    with path.open("a+b") as stream:
        try:
            if os.name == "nt":
                import msvcrt
                stream.seek(0)
                if not stream.read(1):
                    stream.write(b"\0")
                    stream.flush()
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise RuntimeError(f"Dependency preparation is already running for {project}") from error
        try:
            yield
        finally:
            if os.name == "nt":
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
