# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
import threading
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Optional, Tuple


class EventBus:
    def __init__(self) -> None:
        self._handlers: Dict[str, List[Tuple[int, int, Callable[[Any], None]]]] = {}
        self._queue: Deque[Tuple[str, Any]] = deque()
        self._next_id: int = 1
        self._lock = threading.RLock()

    def subscribe(self, event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
        with self._lock:
            token = self._next_id
            self._next_id += 1
            lst = self._handlers.setdefault(event, [])
            lst.append((priority, token, fn))
            lst.sort(key=lambda t: (-t[0], t[1]))
            return token

    def once(self, event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
        holder = {"token": 0}

        def wrapper(payload: Any) -> None:
            try:
                fn(payload)
            finally:
                tok = holder["token"]
                if tok:
                    self.unsubscribe(tok)

        token = self.subscribe(event, wrapper, priority)
        holder["token"] = token
        return token

    def unsubscribe(self, token: int) -> bool:
        with self._lock:
            found = False
            for name, lst in list(self._handlers.items()):
                for i, (_, t, _) in enumerate(lst):
                    if t == token:
                        lst.pop(i)
                        found = True
                        break
                if not lst:
                    self._handlers.pop(name, None)
                if found:
                    break
            return found

    def clear(self, event: Optional[str] = None) -> None:
        with self._lock:
            if event is None:
                self._handlers.clear()
            else:
                self._handlers.pop(event, None)

    def publish(self, event: str, payload: Any = None) -> None:
        with self._lock:
            snapshot = list(self._handlers.get(event, []))
        for _, _, fn in snapshot:
            try:
                fn(payload)
            except Exception as e:
                logging.error(f"EventBus: handler error for '{event}': {e}")

    def post(self, event: str, payload: Any = None) -> None:
        with self._lock:
            self._queue.append((event, payload))

    def flush(self, limit: Optional[int] = None) -> int:
        processed = 0
        while True:
            with self._lock:
                if not self._queue:
                    break
                if limit is not None and processed >= limit:
                    break
                event, payload = self._queue.popleft()
            self.publish(event, payload)
            processed += 1
        return processed


_default_bus = EventBus()


def subscribe(event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
    return _default_bus.subscribe(event, fn, priority)


def once(event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
    return _default_bus.once(event, fn, priority)


def unsubscribe(token: int) -> bool:
    return _default_bus.unsubscribe(token)


def publish(event: str, payload: Any = None) -> None:
    _default_bus.publish(event, payload)


def post(event: str, payload: Any = None) -> None:
    _default_bus.post(event, payload)


def flush(limit: Optional[int] = None) -> int:
    return _default_bus.flush(limit)


def clear(event: Optional[str] = None) -> None:
    _default_bus.clear(event)


__all__ = [
    "EventBus",
    "subscribe",
    "once",
    "unsubscribe",
    "publish",
    "post",
    "flush",
    "clear",
]
