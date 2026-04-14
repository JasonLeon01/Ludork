# -*- encoding: utf-8 -*-

import copy
from typing import Union
from Engine import Vector2i, Vector2u, Vector2f
from Engine.Utils import Math
from .. import System

def getRealSize(inSize: Union[Vector2i, Vector2u, Vector2f]):
    if not isinstance(inSize, Vector2i) and not isinstance(inSize, Vector2u):
        assert isinstance(inSize, Vector2f), "inSize must be a Vector2i, Vector2u or Vector2f"
        size = copy.copy(inSize)
    else:
        size = Math.ToVector2f(inSize)

    return size * System.getScale()
