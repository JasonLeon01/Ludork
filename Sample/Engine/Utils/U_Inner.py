# -*- encoding: utf-8 -*-

import os
import sys
from pathlib import Path

def getSavePath(APP_NAME: str) -> str:
    if sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / APP_NAME / "Save"
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.path.join(os.getcwd(), "Save")
