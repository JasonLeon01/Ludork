# -*- encoding: utf-8 -*-

import os
import copy
import importlib
import importlib.util
import sys
import traceback
from typing import Any, Dict, Optional
from Engine.Decorators import GetConfigVars
from Engine.Utils.DataValue import evalDataExpression, resolveAttrValueType, resolveTypedDataValue, shouldEvalValueType


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
            except Exception:
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
                parentClass = self._dict[classData["parent"]]
                classAttrs = copy.deepcopy(classData.get("attrs", {}))
                if not isinstance(classAttrs, dict):
                    classAttrs = {}
                try:
                    from Engine.Gameplay.Components import migrateLegacyComponentAttrs

                    migrateLegacyComponentAttrs(parentClass, classAttrs)
                    classData["attrs"] = classAttrs
                except Exception as e:
                    print(f"Failed to migrate legacy component attributes for {classPath}: {e}", file=sys.stderr)
                configVarRefs = self._getConfigVarRefs(parentClass)
                self._resolveConfigAttrValues(parentClass, classAttrs, configVarRefs)
                for key, value in classAttrs.items():
                    attrs[key] = value

                def _cloneAttrValue(key: str, value: Any) -> Any:
                    targetType = resolveAttrValueType(parentClass, key)
                    if isinstance(value, str) and shouldEvalValueType(targetType):
                        return evalDataExpression(value)
                    if targetType is not Any:
                        return copy.deepcopy(resolveTypedDataValue(value, targetType))
                    return copy.deepcopy(value)

                def __init__(self, *args, **kwargs) -> None:
                    for key, value in classAttrs.items():
                        if key in self.__dict__:
                            continue
                        setattr(self, key, _cloneAttrValue(key, value))
                    parentClass.__init__(self, *args, **kwargs)
                    try:
                        from Engine.Gameplay.Components import normaliseInstanceComponents

                        normaliseInstanceComponents(self)
                    except Exception as e:
                        print(f"Failed to normalise components for {classPath}: {e}", file=sys.stderr)

                attrs["__init__"] = __init__
                targetClass = type(
                    classPath.replace(".", "_"),
                    (parentClass,),
                    attrs,
                )
                self._dict[classPath] = targetClass
        return self._dict.get(classPath)

    def _getConfigVarRefs(self, cls: type) -> Dict[str, tuple[str, str]]:
        result: Dict[str, tuple[str, str]] = {}
        for base in reversed(cls.mro()):
            meta = getattr(base, "__dict__", {}).get("_meta")
            result.update(GetConfigVars(meta))
        return result

    def _resolveConfigAttrValues(
        self, parentClass: type, classAttrs: Dict[str, Any], configVarRefs: Dict[str, tuple[str, str]]
    ) -> None:
        for key, ref in configVarRefs.items():
            if key in classAttrs:
                classAttrs[key] = self._resolveConfigStringValue(classAttrs[key], ref)
                continue
            parentValue = getattr(parentClass, key, "")
            resolved = self._resolveConfigStringValue(parentValue, ref)
            if resolved != parentValue:
                classAttrs[key] = resolved

    def _resolveConfigStringValue(self, value: Any, ref: tuple[str, str]) -> Any:
        if not isinstance(value, str) or value:
            return value
        try:
            from Source.System import System

            resolved = System.getConfigValue(ref[0], ref[1])
        except Exception:
            return value
        return resolved if isinstance(resolved, str) else str(resolved)

    def invalidate(self, classPath: str) -> None:
        r"""Drop cached class and data entries for the given dot-path.

        - \param classPath Dot-separated class path to evict from cache
        """
        self._dict.pop(classPath, None)
        self._dataDict.pop(classPath, None)

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
