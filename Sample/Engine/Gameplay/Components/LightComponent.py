# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple

from ...Decorators import Meta
from .Component import Component


@Meta(
    ColourVars=["lightColour"],
    VariableDisplayNames={
        "lightColour": 'LOC("LIGHT_COMP_VAR_LIGHT_COLOUR")',
        "lightRadius": 'LOC("LIGHT_COMP_VAR_LIGHT_RADIUS")',
    },
    VariableDisplayDescs={
        "lightColour": 'LOC("LIGHT_COMP_VAR_LIGHT_COLOUR_DESC")',
        "lightRadius": 'LOC("LIGHT_COMP_VAR_LIGHT_RADIUS_DESC")',
    },
)
@dataclass
class LightComponent(Component):
    r"""\brief Self-light settings attached to an actor."""

    lightColour: Tuple[int, int, int, int] = (255, 255, 255, 255)  #: Self light colour
    lightRadius: float = 16.0  #: Self light radius in pixels
