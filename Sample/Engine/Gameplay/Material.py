# -*- encoding: utf-8 -*-

from dataclasses import dataclass, asdict
from typing import Dict, Any


@dataclass
class Material:
    r"""Defines how an actor or tile interacts with lighting, rendering, and movement.

    Applied per-actor or per-tile to control visual and gameplay properties.
    """

    lightBlock: float = 0.0  #: Amount of light blocked (0.0 = transparent, 1.0 = fully opaque)
    mirror: bool = False  #: Whether the surface reflects
    reflectionStrength: float = 0.5  #: Intensity of reflection if mirrored
    opacity: float = 1.0  #: Visual opacity (0.0 = invisible, 1.0 = fully visible)
    speedRate: float = 1.0  #: Movement speed multiplier for actors on this surface

    def asDict(self) -> Dict[str, Any]:
        r"""Serialize the material to a dictionary.

        - \return  Dictionary containing all material fields
        """
        return asdict(self)
