# -*- encoding: utf-8 -*-

import copy
from typing import Union
from . import RenderStates, BlendMode, Vector2i, Vector2u, Vector2f


def CanvasRenderStates() -> RenderStates:
    return RenderStates(
        BlendMode(
            BlendMode.Factor.SrcAlpha,
            BlendMode.Factor.OneMinusSrcAlpha,
            BlendMode.Equation.Add,
            BlendMode.Factor.One,
            BlendMode.Factor.OneMinusSrcAlpha,
            BlendMode.Equation.Add,
        )
    )


def getRealSize(inSize: Union[Vector2i, Vector2u, Vector2f]):
    if not isinstance(inSize, Vector2i) and not isinstance(inSize, Vector2u):
        assert isinstance(inSize, Vector2f), "inSize must be a Vector2i, Vector2u or Vector2f"
        size = copy.copy(inSize)
    else:
        from ..Utils import Math

        size = Math.ToVector2f(inSize)

    from .. import System

    return size * System.getScale()
