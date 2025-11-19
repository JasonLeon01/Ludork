# -*- encoding: utf-8 -*-

from .. import pysf

RenderStates = pysf.RenderStates
BlendMode = pysf.BlendMode


class ModifiedRenderStates(RenderStates):
    @staticmethod
    def Default() -> RenderStates:
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
