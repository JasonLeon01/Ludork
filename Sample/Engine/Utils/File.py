# -*- encoding: utf-8 -*-

import json
import pickle
from typing import Dict, Any


def getJSONData(filePath: str) -> Dict[str, Any]:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Load and parse a JSON file.

    - \param filePath Path to the JSON file to load.
    - \return Parsed JSON data as a dictionary.
    """
    with open(filePath, "r", encoding="utf-8") as file:
        jsonData = file.read()
    return json.loads(jsonData)


def saveData(filePath: str, data: Any) -> None:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Save data to a file using pickle serialisation.

    - \param filePath Path to the output file.
    - \param data Data to be serialised and saved.
    """
    with open(filePath, "wb") as file:
        pickle.dump(data, file)


def loadData(filePath: str) -> Any:
    r"""////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    \brief Load data from a pickle file.

    - \param filePath Path to the pickle file to load.
    - \return Deserialised data.
    """
    with open(filePath, "rb") as file:
        return pickle.load(file)
