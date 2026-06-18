# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import List, Optional

from EditorGlobal import EditorStatus, GameData

BLUEPRINT_PREVIEW_BASE_CLASSES: List[str] = [
    "Engine.Gameplay.Actors.Actor",
]


def getBlueprintPreviewBaseClasses() -> List[type]:
    r"""Resolve configured preview base classes from ``BLUEPRINT_PREVIEW_BASE_CLASSES``.

    - \return Loaded base types that support blueprint texture preview
    """
    bases: List[type] = []
    for classPath in BLUEPRINT_PREVIEW_BASE_CLASSES:
        if not isinstance(classPath, str) or not classPath.strip():
            continue
        try:
            cls = GameData.classDict.get(classPath.strip(), EditorStatus.PROJ_PATH)
        except Exception:
            continue
        if isinstance(cls, type):
            bases.append(cls)
    return bases


def isBlueprintPreviewable(cls: Optional[type]) -> bool:
    r"""Return whether a blueprint class supports the editor preview tab.

    - \param cls Resolved blueprint class, or ``None`` when unavailable
    - \return ``True`` when ``cls`` inherits any configured preview base class
    """
    if not isinstance(cls, type):
        return False
    bases = getBlueprintPreviewBaseClasses()
    if not bases:
        return False
    return any(issubclass(cls, base) for base in bases)
