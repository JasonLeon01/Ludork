# -*- encoding: utf-8 -*-

from __future__ import annotations

import logging
from typing import List, Optional

from EditorGlobal import EditorStatus, GameData

log = logging.getLogger(__name__)

BLUEPRINT_PREVIEW_BASE_CLASSES: List[str] = [
    "Engine.Gameplay.Actors.Actor",
]


def GetBlueprintPreviewBaseClasses() -> List[type]:
    bases: List[type] = []
    for classPath in BLUEPRINT_PREVIEW_BASE_CLASSES:
        if not isinstance(classPath, str) or not classPath.strip():
            continue
        try:
            cls = GameData.classDict.get(classPath.strip(), EditorStatus.PROJ_PATH)
        except Exception as e:
            log.warning("Failed to resolve blueprint preview base class %s: %s", classPath, e)
            continue
        if isinstance(cls, type):
            bases.append(cls)
    return bases


def IsBlueprintPreviewable(cls: Optional[type]) -> bool:
    if not isinstance(cls, type):
        return False
    bases = GetBlueprintPreviewBaseClasses()
    if not bases:
        return False
    return any(issubclass(cls, base) for base in bases)
