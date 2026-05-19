# -*- encoding: utf-8 -*-
r"""
\brief Fog package.

Provides RMXP-style full-screen map fog with texture scrolling on iOS
and optional UV distortion via shader on desktop.
"""

from .FogController import FogController

__all__ = ["FogController"]
