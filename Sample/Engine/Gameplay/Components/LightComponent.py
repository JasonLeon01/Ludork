# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple

from .Component import Component


@dataclass
class LightComponent(Component):
    r"""\brief Self-light settings attached to an actor."""

    lightColor: Tuple[int, int, int, int] = (255, 255, 255, 255)  #: Self light colour
    lightRadius: float = 16.0  #: Self light radius in pixels
