# -*- encoding: utf-8 -*-

import json
from typing import Dict, Any


def getJSONData(filePath: str) -> Dict[str, Any]:
    with open(filePath, "r", encoding="utf-8") as file:
        jsonData = file.read()
    return json.loads(jsonData)
