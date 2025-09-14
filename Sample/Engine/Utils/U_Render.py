# -*- encoding: utf-8 -*-

from . import RenderStates, BlendMode


def CanvasRenderState() -> RenderStates:
    return RenderStates(
        BlendMode(
            BlendMode.Factor.One,
            BlendMode.Factor.OneMinusSrcAlpha,
            BlendMode.Equation.Add,
            BlendMode.Factor.One,
            BlendMode.Factor.OneMinusSrcAlpha,
            BlendMode.Equation.Add,
        )
    )
