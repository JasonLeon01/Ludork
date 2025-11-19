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
    Vector2u,
    RectangleShape,
)
from . import UI_SpriteBase
from . import UI_Canvas
from . import UI_Text
from . import UI_Window
from . import UI_Rect

SpriteBase = UI_SpriteBase.SpriteBase
Canvas = UI_Canvas.Canvas
TextStyle = UI_Text.TextStyle
PlainText = UI_Text.PlainText
RichText = UI_Text.RichText
Window = UI_Window.Window
Rect = UI_Rect.Rect
