# -*- encoding: utf-8 -*-

from .. import (
    Sprite,
    Texture,
    Image,
    IntRect,
    FloatRect,
    Vector2i,
    Vector2f,
    Vector2u,
    RenderTexture,
    View,
    Angle,
    degrees,
    Time,
    seconds,
    Utils,
    Color,
    Drawable,
    Transformable,
    Text,
    Font,
    RenderStates,
    RenderTarget,
    VertexArray,
    PrimitiveType,
)

from . import UI_Canvas
from . import UI_RichText
from . import UI_Window

Canvas = UI_Canvas.UI
RichText = UI_RichText.UI
TextStroke = UI_RichText.TextStroke
Outline = UI_RichText.Outline
Window = UI_Window.UI
