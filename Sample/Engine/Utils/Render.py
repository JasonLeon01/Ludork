# -*- encoding: utf-8 -*-

from .. import RenderStates, BlendMode


def CanvasRenderStates() -> RenderStates:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Create a RenderStates object configured for typical canvas rendering.

    Uses alpha blending (src alpha / one minus src alpha) for both
    colour and alpha channels.

    - \return RenderStates object with pre-configured blend mode.
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
