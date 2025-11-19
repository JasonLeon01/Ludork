# -*- encoding: utf-8 -*-

import copy
from ..pysf import RenderStates, BlendMode


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
