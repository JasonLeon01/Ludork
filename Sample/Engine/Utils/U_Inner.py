# -*- encoding: utf-8 -*-

import os
import sys
import re
from pathlib import Path
from typing import Any, Dict
from ..Locale import getContent


def getSavePath(APP_NAME: str) -> str:
    if sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / APP_NAME / "Save"
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.path.join(os.getcwd(), "Save")


def filterDataClassParams(params: Dict[str, Any], type_: type) -> Dict[str, Any]:
    return {k: v for k, v in params.items() if hasattr(type_, k)}


def ApplyStringLocaleFormat(string: str) -> str:
    pattern = r"\{(.*?)\}"
    matches = re.findall(pattern, string)
    return string.format(**{match: getContent(match) for match in matches})
