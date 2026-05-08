# -*- encoding: utf-8 -*-

import os
import copy
import importlib
import importlib.util
import traceback
from typing import Any, Dict, Optional


class ClassDict:
    r"""Dynamic class loader and cache.

    Resolves classes by dot-path, loading from Python modules
    or from .dat/.json data files when no module is found.
    """

    def __init__(self) -> None:
        r"""(Re)initialise the class dictionary.

        - \brief Creates empty caches for classes and class data.
        """
        self._dict: Dict[str, Any] = {"": object}
        self._dataDict: Dict[str, Dict[str, Any]] = {}

    def get(self, classPath: Optional[str], root: Optional[str] = None) -> Any:
        r"""Resolve and return a class by its dot-path.

        Attempts to load from a Python module first; falls back to
        loading class data from .dat or .json files.

        - \param classPath  Dot-separated class path (e.g. "Source.Scenes.MyScene")
        - \param root       Optional root directory for data files
        - \return Resolved class, or None if classPath is None
        """
        from Engine.Utils import File

        if classPath is None:
            return None

        if not classPath in self._dict:
            modulePath, className = classPath.rsplit(".", 1)
            loadDataClass = False
            try:
                moduleSpec = importlib.util.find_spec(modulePath)
            except:
                loadDataClass = True
            if not loadDataClass:
                module = None
                if (
                    not moduleSpec is None
                    and not moduleSpec.origin is None
                    and (
                        moduleSpec.origin.endswith(".py")
                        or moduleSpec.origin.endswith(".pyd")
                        or moduleSpec.origin.endswith(".so")
                    )
                ):
                    try:
                        module = importlib.import_module(modulePath)
                    except Exception as e:
                        raise ImportError(f"Error loading module {modulePath}: {e}, detail: {traceback.format_exc()}")
                    if hasattr(module, className):
                        self._dict[classPath] = getattr(module, className)
                    else:
                        loadDataClass = True
                else:
                    loadDataClass = True
            if loadDataClass:
                filePath = classPath.replace(".", "/")
                if not root is None:
                    filePath = os.path.join(root, filePath)
                classData = None
                if os.path.exists(filePath + ".dat"):
                    classData = File.loadData(filePath + ".dat")
                elif os.path.exists(filePath + ".json"):
                    classData = File.getJSONData(filePath + ".json")
                else:
                    raise ImportError(f"Class {classPath} not found")
                self._dataDict[classPath] = classData
                if not classData["parent"] in self._dict:
                    self.get(classData["parent"], root)
                attrs = {"_GENERATED_CLASS": True}
                classAttrs = classData.get("attrs", {})
                for key, value in classAttrs.items():
                    attrs[key] = value

                def _cloneAttrValue(value: Any) -> Any:
                    if isinstance(value, str):
                        try:
                            return eval(value)
                        except:
                            return value
                    return copy.deepcopy(value)

                def __init__(self, *args, **kwargs) -> None:
                    for key, value in classAttrs.items():
                        setattr(self, key, _cloneAttrValue(value))
                    super(type(self), self).__init__(*args, **kwargs)
                    for key, value in classAttrs.items():
                        setattr(self, key, _cloneAttrValue(value))

                attrs["__init__"] = __init__
                targetClass = type(
                    classPath.replace(".", "_"),
                    (self._dict[classData["parent"]],),
                    attrs,
                )
                self._dict[classPath] = targetClass
        return self._dict.get(classPath)

    def getData(self, classPath: str) -> Dict[str, Any]:
        r"""Get the raw class data dictionary for a previously loaded class.

        - \param classPath  Dot-separated class path
        - \return Class data dictionary, or empty dict if not found
        """
        return self._dataDict.get(classPath, {})

    def __getitem__(self, classPath: str) -> Any:
        r"""Allow dictionary-style access to resolve classes.

        - \param classPath  Dot-separated class path
        - \return Resolved class
        """
        return self.get(classPath)
