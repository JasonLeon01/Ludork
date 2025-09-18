# -*- encoding: utf-8 -*-

from Engine import pysf

sfRenderStates = pysf.RenderStates
sfBlendMode = pysf.BlendMode


class ModifiedRenderStates(sfRenderStates):
    @staticmethod
    def Default():
        return sfRenderStates(
            sfBlendMode(
                sfBlendMode.Factor.SrcAlpha,
                sfBlendMode.Factor.OneMinusSrcAlpha,
                sfBlendMode.Equation.Add,
                sfBlendMode.Factor.One,
                sfBlendMode.Factor.OneMinusSrcAlpha,
                sfBlendMode.Equation.Add,
            )
        )
