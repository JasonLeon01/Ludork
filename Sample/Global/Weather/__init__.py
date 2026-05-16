# -*- encoding: utf-8 -*-
r"""
\brief Weather package.

Provides RMXP-style screen weather with shader rendering on desktop
and particle fallback on iOS where custom shaders cannot be loaded.
"""

from .WeatherController import WeatherController, WeatherType

__all__ = ["WeatherController", "WeatherType"]
