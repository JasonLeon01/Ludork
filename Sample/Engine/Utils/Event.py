# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
import threading
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Optional, Tuple


class EventBus:
    r"""
    \brief Thread-safe event bus with priority-based handlers.

    Supports subscribing, unsubscribing, one-shot handlers,
    immediate publishing, and queued (deferred) event processing.
    """

    def __init__(self) -> None:
        r"""
        \brief Construct a event bus.
        """

        self._handlers: Dict[str, List[Tuple[int, int, Callable[[Any], None]]]] = {}
        self._queue: Deque[Tuple[str, Any]] = deque()
        self._next_id: int = 1
        self._lock = threading.RLock()
        self._subscriptionIndex: Dict[str, int] = {}
        self._tokenIndex: Dict[int, str] = {}

    def _trackSubscription(self, token: int, event: str) -> None:
        self._subscriptionIndex[event] = token
        self._tokenIndex[token] = event

    def _dropSubscriptionIndex(self, token: int) -> None:
        event = self._tokenIndex.pop(token, None)
        if event is not None:
            self._subscriptionIndex.pop(event, None)

    def _removeHandlerToken(self, token: int) -> bool:
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

    def subscribeObjectHandler(
        self,
        event: str,
        obj: object,
        fn: Callable[[Any], None],
        priority: int = 0,
    ) -> int:
        r"""
        \brief Subscribe a handler and index it by event key.

        - \param event Event name to subscribe to.
        - \param obj Target object associated with the handler.
        - \param fn Callback function accepting one payload argument.
        - \param priority Priority value (higher = earlier); default 0.
        - \return Subscription token that can be used to unsubscribe.
        """
        with self._lock:
            oldToken = self._subscriptionIndex.pop(event, None)
            if oldToken is not None:
                self._tokenIndex.pop(oldToken, None)
                self._removeHandlerToken(oldToken)
            token = self._next_id
            self._next_id += 1
            lst = self._handlers.setdefault(event, [])
            lst.append((priority, token, fn))
            lst.sort(key=lambda t: (-t[0], t[1]))
            self._trackSubscription(token, event)
            return token

    def subscribe(self, event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
        r"""
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
        r"""
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

    def subscribeBlueprintEvent(self, event: str, obj: object, eventName: str, priority: int = 0) -> int:
        r"""
        \brief Subscribe a blueprint event on an object to an EventBus event.

        The EventBus payload is ignored; blueprint event arguments are not passed.

        - \param event Event name to subscribe to.
        - \param obj Target object that owns the blueprint event.
        - \param eventName Blueprint event name to invoke.
        - \param priority Priority value (higher = earlier); default 0.
        - \return Subscription token that can be used to unsubscribe.
        """
        _validateBlueprintEventTarget(obj, eventName)

        def handler(_: Any) -> None:
            triggerBlueprintEvent(obj, eventName)

        return self.subscribeObjectHandler(event, obj, handler, priority)

    def onceBlueprintEvent(self, event: str, obj: object, eventName: str, priority: int = 0) -> int:
        r"""
        \brief Subscribe a one-shot blueprint event handler.

        The EventBus payload is ignored; blueprint event arguments are not passed.

        - \param event Event name to subscribe to.
        - \param obj Target object that owns the blueprint event.
        - \param eventName Blueprint event name to invoke.
        - \param priority Priority value (higher = earlier); default 0.
        - \return Subscription token that can be used to unsubscribe before invocation.
        """
        _validateBlueprintEventTarget(obj, eventName)

        def handler(_: Any) -> None:
            triggerBlueprintEvent(obj, eventName)

        return self.once(event, handler, priority)

    def unsubscribe(self, token: int) -> bool:
        r"""
        \brief Unsubscribe a handler using its subscription token.

        - \param token Token returned by subscribe() or once().
        - \return True if the token was found and removed, False otherwise.
        """
        with self._lock:
            self._dropSubscriptionIndex(token)
            return self._removeHandlerToken(token)

    def unsubscribeEvent(self, event: str) -> bool:
        r"""
        \brief Unsubscribe all handlers for an event key.

        - \param event Event name subscribed to.
        - \return True if any handler was found and removed, False otherwise.
        """
        with self._lock:
            hadHandlers = event in self._handlers
            self._handlers.pop(event, None)
            token = self._subscriptionIndex.pop(event, None)
            if token is not None:
                self._tokenIndex.pop(token, None)
            return hadHandlers

    def clear(self, event: Optional[str] = None) -> None:
        r"""
        \brief Remove all handlers, or only those for a specific event.

        - \param event Event name to clear; if None, all events are cleared.
        """
        with self._lock:
            if event is None:
                self._handlers.clear()
                self._subscriptionIndex.clear()
                self._tokenIndex.clear()
            else:
                self._handlers.pop(event, None)
                token = self._subscriptionIndex.pop(event, None)
                if token is not None:
                    self._tokenIndex.pop(token, None)

    def publish(self, event: str, payload: Any = None) -> None:
        r"""
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
        r"""
        \brief Post an event to the deferred queue (processed later by flush).

        - \param event Event name to post.
        - \param payload Payload to associate with the event.
        """
        with self._lock:
            self._queue.append((event, payload))

    def flush(self, limit: Optional[int] = None) -> int:
        r"""
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


def _validateBlueprintEventTarget(obj: object, eventName: str) -> None:
    if obj is None:
        raise ValueError("Blueprint event target object is None")
    graph = getattr(obj, "_graph", None)
    if graph is not None and hasattr(graph, "hasKey") and graph.hasKey(eventName):
        return
    infoGraph = getattr(obj, "_infoGraph", None)
    if infoGraph is not None and hasattr(infoGraph, "hasKey") and infoGraph.hasKey(eventName):
        return
    if hasattr(obj, eventName) and callable(getattr(obj, eventName)):
        return
    else:
        raise AttributeError(f"Object has no blueprint event '{eventName}'")


def triggerBlueprintEvent(obj: object, eventName: str) -> None:
    r"""
    \brief Trigger a blueprint event on an object without arguments.

    - \param obj Target object that owns the blueprint event.
    - \param eventName Blueprint event name to invoke.
    """
    _validateBlueprintEventTarget(obj, eventName)
    from Engine.BPBase import BPBase

    if getattr(obj, "isDestroyed", lambda: False)():
        return
    graph = getattr(obj, "_graph", None)
    if (
        graph is not None
        and hasattr(graph, "hasKey")
        and graph.hasKey(eventName)
        and hasattr(graph, "startNodes")
        and eventName in graph.startNodes
        and graph.startNodes[eventName] is not None
    ):
        BPBase._executeGraph(graph, eventName, {})
        return
    if BPBase._tryExecuteInfoGraph(obj, eventName, {}):
        return
    BPBase.BlueprintEvent(obj, type(obj), eventName)


_default_bus = EventBus()


def subscribe(event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
    r"""
    \brief Subscribe to an event on the default event bus.

    - \param event Event name to subscribe to.
    - \param fn Callback function accepting one payload argument.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe.
    """
    return _default_bus.subscribe(event, fn, priority)


def once(event: str, fn: Callable[[Any], None], priority: int = 0) -> int:
    r"""
    \brief Subscribe a one-shot handler on the default event bus.

    - \param event Event name to subscribe to.
    - \param fn Callback function accepting one payload argument.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe before invocation.
    """
    return _default_bus.once(event, fn, priority)


def subscribeBlueprintEvent(event: str, obj: object, eventName: str, priority: int = 0) -> int:
    r"""
    \brief Subscribe a blueprint event on the default event bus.

    - \param event Event name to subscribe to.
    - \param obj Target object that owns the blueprint event.
    - \param eventName Blueprint event name to invoke.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe.
    """
    return _default_bus.subscribeBlueprintEvent(event, obj, eventName, priority)


def subscribeObjectHandler(
    event: str,
    obj: object,
    fn: Callable[[Any], None],
    priority: int = 0,
) -> int:
    r"""
    \brief Subscribe an indexed object handler on the default event bus.

    - \param event Event name to subscribe to.
    - \param obj Target object associated with the handler.
    - \param fn Callback function accepting one payload argument.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe.
    """
    return _default_bus.subscribeObjectHandler(event, obj, fn, priority)


def onceBlueprintEvent(event: str, obj: object, eventName: str, priority: int = 0) -> int:
    r"""
    \brief Subscribe a one-shot blueprint event on the default event bus.

    - \param event Event name to subscribe to.
    - \param obj Target object that owns the blueprint event.
    - \param eventName Blueprint event name to invoke.
    - \param priority Priority value (higher = earlier); default 0.
    - \return Subscription token that can be used to unsubscribe before invocation.
    """
    return _default_bus.onceBlueprintEvent(event, obj, eventName, priority)


def unsubscribe(token: int) -> bool:
    r"""
    \brief Unsubscribe a handler on the default event bus.

    - \param token Token returned by subscribe() or once().
    - \return True if the token was found and removed, False otherwise.
    """
    return _default_bus.unsubscribe(token)


def unsubscribeEvent(event: str) -> bool:
    r"""
    \brief Unsubscribe all handlers for an event key on the default event bus.

    - \param event Event name subscribed to.
    - \return True if any handler was found and removed, False otherwise.
    """
    return _default_bus.unsubscribeEvent(event)


def publish(event: str, payload: Any = None) -> None:
    r"""
    \brief Immediately publish an event on the default event bus.

    - \param event Event name to publish.
    - \param payload Payload to pass to each handler.
    """
    _default_bus.publish(event, payload)


def post(event: str, payload: Any = None) -> None:
    r"""
    \brief Post an event to the deferred queue on the default event bus.

    - \param event Event name to post.
    - \param payload Payload to associate with the event.
    """
    _default_bus.post(event, payload)


def flush(limit: Optional[int] = None) -> int:
    r"""
    \brief Process queued events on the default event bus.

    - \param limit Maximum number of events to process; None means no limit.
    - \return Number of events processed.
    """
    return _default_bus.flush(limit)


def clear(event: Optional[str] = None) -> None:
    r"""
    \brief Clear handlers on the default event bus.

    - \param event Event name to clear; if None, all events are cleared.
    """
    _default_bus.clear(event)


__all__ = [
    "EventBus",
    "subscribe",
    "once",
    "subscribeBlueprintEvent",
    "subscribeObjectHandler",
    "onceBlueprintEvent",
    "unsubscribe",
    "unsubscribeEvent",
    "publish",
    "post",
    "flush",
    "clear",
    "triggerBlueprintEvent",
]
