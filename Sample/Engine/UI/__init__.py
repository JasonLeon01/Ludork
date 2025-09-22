# -*- encoding: utf-8 -*-

from .. import (
    Sprite,
    Vector2f,
    Angle,
    degrees,
    IntRect,
    Vector2i,
    RenderTexture,
    Color,
    Utils,
    Drawable,
    Transformable,
    Text,
    FloatRect,
    RenderStates,
    Texture,
    View,
)
from . import UI_SpriteBase
from . import UI_Canvas
from . import UI_RichText
from . import UI_Window

SpriteBase = UI_SpriteBase.SpriteBase
Canvas = UI_Canvas.Canvas
RichText = UI_RichText.RichText
TextStroke = UI_RichText.TextStroke
Outline = UI_RichText.Outline
Window = UI_Window.Window
