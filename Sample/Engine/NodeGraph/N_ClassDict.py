# -*- encoding: utf-8 -*-

import os
import importlib
import importlib.util
from typing import Any, Dict, Optional


class ClassDict:
    def __init__(self):
        self._dict: Dict[str, Any] = {"": object}
        self._dataDict: Dict[str, Dict[str, Any]] = {}

    def get(self, classPath: Optional[str], root: Optional[str] = None) -> Any:
        from Engine.Utils import File

        if classPath is None:
            return None

        if not classPath in self._dict:
            modulePath, className = classPath.rsplit(".", 1)
            loadDataClass = False
            try:
                moduleSpec = importlib.util.find_spec(modulePath)
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
                    module = importlib.import_module(modulePath)
                    if hasattr(module, className):
                        self._dict[classPath] = getattr(module, className)
                    else:
                        loadDataClass = True
                else:
                    loadDataClass = True
            except:
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

                def __init__(self, *args, **kwargs):
                    super(type(self), self).__init__(*args, **kwargs)
                    for key, value in classAttrs.items():
                        try:
                            setattr(self, key, eval(value))
                        except:
                            setattr(self, key, value)

                attrs["__init__"] = __init__
                targetClass = type(
                    classPath.replace(".", "_"),
                    (self._dict[classData["parent"]],),
                    attrs,
                )
                self._dict[classPath] = targetClass
        return self._dict.get(classPath)

    def getData(self, classPath: str) -> Dict[str, Any]:
        return self._dataDict.get(classPath, {})

    def __getitem__(self, classPath: str) -> Any:
        return self.get(classPath)
