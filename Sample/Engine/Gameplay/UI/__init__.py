# -*- encoding: utf-8 -*-

from .. import (
    Sprite,
    IntRect,
    FloatRect,
    Vector2i,
    RenderTexture,
    Vector2f,
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
)
from . import UI_Base
from . import UI_RichText

Base = UI_Base.UI
RichText = UI_RichText.UI
TextStroke = UI_RichText.TextStroke
Outline = UI_RichText.Outline
