# -*- encoding: utf-8 -*-

from ..pysf import RenderStates, BlendMode


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
