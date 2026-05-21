# -*- encoding: utf-8 -*-

r"""
\brief General reusable window controls package.

Provides configuration-oriented widgets and rows
shared by game windows.

- DropBox           Drop-down field and expanded list widgets
- CheckBox          Boolean checkbox field widget
- Slider            Integer slider field widget
- ConfigSettingRow  Label row with a DropBox control
- ConfigCheckBoxRow Label row with a CheckBox control
- ConfigSliderRow   Label row with a Slider control
"""

from .DropBox import DropBox
from .CheckBox import CheckBox
from .Slider import Slider
from .ConfigSettingRow import ConfigSettingRow
from .ConfigCheckBoxRow import ConfigCheckBoxRow
from .ConfigSliderRow import ConfigSliderRow

__all__ = [
    "DropBox",
    "CheckBox",
    "Slider",
    "ConfigSettingRow",
    "ConfigCheckBoxRow",
    "ConfigSliderRow",
]
