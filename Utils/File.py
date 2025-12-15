# -*- encoding: utf-8 -*-

import json
import pickle
from typing import Dict, Any


def getJSONData(filePath: str) -> Dict[str, Any]:
    with open(filePath, "r", encoding="utf-8") as file:
        jsonData = file.read()
    return json.loads(jsonData)


def saveJsonData(filePath: str, data: Dict[str, Any]) -> None:
    with open(filePath, "w", encoding="utf-8") as file:
        json.dump(data, file, ensure_ascii=False)


def loadData(filePath: str) -> Any:
    with open(filePath, "rb") as file:
        return pickle.load(file)


def saveData(filePath, data: Dict[str, Any]) -> None:
    with open(filePath, "wb") as file:
        pickle.dump(data, file)
