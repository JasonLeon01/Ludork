# -*- encoding: utf-8 -*-

from dataclasses import dataclass, asdict
from typing import Dict, Any


@dataclass
class Material:
    lightBlock: float = 0.0
    mirror: bool = False
    reflectionStrength: float = 0.5
    opacity: float = 1.0
    emissive: float = 0.0
    speedRate: float = 1.0

    def asDict(self) -> Dict[str, Any]:
        return asdict(self)
