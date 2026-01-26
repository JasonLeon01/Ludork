# -*- encoding: utf-8 -*-

import os
import sys
from pathlib import Path
import json
import pickle
from typing import Dict, Any


def getJSONData(filePath: str) -> Dict[str, Any]:
    with open(filePath, "r", encoding="utf-8") as file:
        jsonData = file.read()
    return json.loads(jsonData)


def saveData(filePath: str, data: Any) -> None:
    with open(filePath, "wb") as file:
        pickle.dump(data, file)


def loadData(filePath: str) -> Any:
    with open(filePath, "rb") as file:
        return pickle.load(file)


def getSavePath() -> str:
    import EditorStatus

    if sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / EditorStatus.APP_NAME / "Save"
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.path.join(os.getcwd(), "Save")
