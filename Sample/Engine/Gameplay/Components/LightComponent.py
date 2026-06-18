# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple

from ...Decorators import Meta
from .Component import Component


@Meta(ColourVars=["lightColour"])
@dataclass
class LightComponent(Component):
    r"""\brief Self-light settings attached to an actor."""

    lightColour: Tuple[int, int, int, int] = (255, 255, 255, 255)  #: Self light colour
    lightRadius: float = 16.0  #: Self light radius in pixels
