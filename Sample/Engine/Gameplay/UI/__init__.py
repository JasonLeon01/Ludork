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

SpriteBase = UI_SpriteBase.UI
Canvas = UI_Canvas.UI
RichText = UI_RichText.UI
TextStroke = UI_RichText.TextStroke
Outline = UI_RichText.Outline
Window = UI_Window.UI
