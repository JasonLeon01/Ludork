# -*- encoding: utf-8 -*-
# Additional builtins injected at runtime by __init__.py

from typing import Any, Callable, TypeVar

_T = TypeVar("_T")
_F = TypeVar("_F", bound=Callable[..., Any])

def ELOC(key: str) -> str: ...
