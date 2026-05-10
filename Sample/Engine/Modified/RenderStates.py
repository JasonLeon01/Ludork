# -*- encoding: utf-8 -*-

from pysf import RenderStates, BlendMode


class ModifiedRenderStates(RenderStates):
    r"""
    \brief Modified Render States.
    """

    @staticmethod
    def Default() -> RenderStates:
        r"""
        \brief Default.
        """

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
