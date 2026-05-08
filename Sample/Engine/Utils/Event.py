# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
import threading
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Optional, Tuple


class EventBus:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Thread-safe event bus with priority-based handlers.

    Supports subscribing, unsubscribing, one-shot handlers,
    immediate publishing, and queued (deferred) event processing.
    """

    def __init__(self) -> None:
        self._handlers: Dict[str, List[Tuple[int, int, Callable[[Any], None]]]] = {}
        self._queue: Deque[Tuple[str, Any]] = deque()
        self._next_id: int = 1
        self._lock = threading.RLock()

    def subscribe(self, event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Subscribe a handler to an event with an optional priority.

        Higher priority handlers are called first.
        Tie-breaking is by subscription order (ascending token).

        - \param event Event name to subscribe to.
        - \param fn Callback function accepting one payload argument.
        - \param priority Priority value (higher = earlier); default 0.
        - \return Subscription token that can be used to unsubscribe.
        """
        with self._lock:
            token = self._next_id
            self._next_id += 1
            lst = self._handlers.setdefault(event, [])
            lst.append((priority, token, fn))
            lst.sort(key=lambda t: (-t[0], t[1]))
            return token

    def once(self, event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Subscribe a one-shot handler that auto-removes after first invocation.

        - \param event Event name to subscribe to.
        - \param fn Callback function accepting one payload argument.
        - \param priority Priority value (higher = earlier); default 0.
        - \return Subscription token that can be used to unsubscribe before invocation.
        """
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
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Unsubscribe a handler using its subscription token.

        - \param token Token returned by subscribe() or once().
        - \return True if the token was found and removed, False otherwise.
        """
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
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Remove all handlers, or only those for a specific event.

        - \param event Event name to clear; if None, all events are cleared.
        """
        with self._lock:
            if event is None:
                self._handlers.clear()
            else:
                self._handlers.pop(event, None)

    def publish(self, event: str, payload: Any = None) -> None:
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Immediately publish an event to all subscribed handlers.

        Handlers are called synchronously in priority order.
        Exceptions in handlers are logged but do not prevent other handlers.

        - \param event Event name to publish.
        - \param payload Payload to pass to each handler.
        """
        with self._lock:
            snapshot = list(self._handlers.get(event, []))
        for _, _, fn in snapshot:
            try:
                fn(payload)
            except Exception as e:
                logging.error(f"EventBus: handler error for '{event}': {e}")

    def post(self, event: str, payload: Any = None) -> None:
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Post an event to the deferred queue (processed later by flush).

        - \param event Event name to post.
        - \param payload Payload to associate with the event.
        """
        with self._lock:
            self._queue.append((event, payload))

    def flush(self, limit: Optional[int] = None) -> int:
        r"""////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////
        \brief Process queued events up to an optional limit.

        - \param limit Maximum number of events to process; None means no limit.
        - \return Number of events processed.
        """
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
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Subscribe to an event on the default event bus.

    - \param event Event name to subscribe to.
    - \param fn Callback function accepting one payload argument.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe.
    """
    return _default_bus.subscribe(event, fn, priority)


def once(event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Subscribe a one-shot handler on the default event bus.

    - \param event Event name to subscribe to.
    - \param fn Callback function accepting one payload argument.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe before invocation.
    """
    return _default_bus.once(event, fn, priority)


def unsubscribe(token: int) -> bool:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Unsubscribe a handler on the default event bus.

    - \param token Token returned by subscribe() or once().
    - \return True if the token was found and removed, False otherwise.
    """
    return _default_bus.unsubscribe(token)


def publish(event: str, payload: Any = None) -> None:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Immediately publish an event on the default event bus.

    - \param event Event name to publish.
    - \param payload Payload to pass to each handler.
    """
    _default_bus.publish(event, payload)


def post(event: str, payload: Any = None) -> None:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Post an event to the deferred queue on the default event bus.

    - \param event Event name to post.
    - \param payload Payload to associate with the event.
    """
    _default_bus.post(event, payload)


def flush(limit: Optional[int] = None) -> int:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Process queued events on the default event bus.

    - \param limit Maximum number of events to process; None means no limit.
    - \return Number of events processed.
    """
    return _default_bus.flush(limit)


def clear(event: Optional[str] = None) -> None:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Clear handlers on the default event bus.

    - \param event Event name to clear; if None, all events are cleared.
    """
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
