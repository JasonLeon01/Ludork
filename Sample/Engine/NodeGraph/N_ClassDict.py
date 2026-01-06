# -*- encoding: utf-8 -*-

import os
import importlib
import importlib.util
from typing import Any, Dict


class ClassDict:
    def __init__(self):
        self._dict: Dict[str, Any] = {}

    def get(self, classPath: str) -> Any:
        from Engine.Utils import File

        if not classPath in self._dict:
            modulePath, className = classPath.rsplit(".", 1)
            moduleSpec = importlib.util.find_spec(modulePath)
            loadDataClass = False
            module = None
            if not moduleSpec is None:
                module = importlib.import_module(modulePath)
                if hasattr(module, className):
                    self._dict[classPath] = getattr(module, className)
                else:
                    loadDataClass = True
            else:
                loadDataClass = True
            if loadDataClass:
                if not os.path.exists(classPath.replace(".", "/") + ".dat"):
                    raise ImportError(f"Class {classPath} not found")
                classData = File.loadData(classPath.replace(".", "/") + ".dat")
                if not classData["parent"] in self._dict:
                    self.get(classData["parent"])
                targetClass = type(
                    classPath.replace(".", "_"),
                    (self._dict[classData["parent"]],),
                    classData["attrs"],
                )
                setattr(targetClass, "GENERATED_CLASS", True)
                self._dict[classPath] = targetClass
        return self._dict.get(classPath)

    def __getitem__(self, classPath: str) -> Any:
        return self.get(classPath)
