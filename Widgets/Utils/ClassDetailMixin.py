# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
import dataclasses
import logging
import os
from typing import Any, Callable, Dict, Optional, Set, get_origin, get_type_hints

from PyQt5 import QtWidgets

from EditorGlobal import EditorStatus
from Widgets.Utils.ColourPickerDialog import ColourVarEditor
from Widgets.Utils.MetaRely import GetRelyConditionDisplay, GetRelySourceSet, IsRelyEditable, NormaliseRelyMap
from Widgets.Utils.MetaVarTypes import _GENERALDATA_VAR_TYPE, GetGeneralDataVars, GetMetaVarTypes
from Widgets.Utils.NodePanel import GeneralDataComboBox, _loadGeneralDataMemberKeys
from Widgets.Utils.StructuredFields import IsStructuredValue, StructuredValueToDict
from Widgets.Utils.TypedValueEditor import TypedValueEditor, UnwrapOptionalType
from Widgets.Utils.VariableNameLabel import VariableNameLabel
from Widgets.Utils.VectorVarEditor import VectorVarEditor, IsVectorVarType


log = logging.getLogger(__name__)


ClassDetailChanged = Callable[[str, Any, bool], None]


class ClassDetailMixin:
    def _resolveClass(self) -> Optional[type]:
        return None

    def _getBaseClass(self, cls: Optional[type]) -> Optional[type]:
        if isinstance(cls, type) and cls.__bases__:
            return cls.__bases__[0]
        return None

    def _getClassAttrValue(self, cls: Optional[type], name: str) -> tuple[bool, Any]:
        if not isinstance(cls, type):
            return False, None
        try:
            return True, getattr(cls, name)
        except AttributeError:
            return False, None
        except Exception:
            return False, None

    def _hasClassAttr(self, cls: Optional[type], name: str) -> bool:
        found, _value = self._getClassAttrValue(cls, name)
        return found

    def _getTypeHints(self, cls: Optional[type]) -> Dict[str, Any]:
        if not isinstance(cls, type):
            return {}
        try:
            return get_type_hints(cls)
        except (NameError, TypeError, AttributeError):
            hints = getattr(cls, "__annotations__", {})
            return hints if isinstance(hints, dict) else {}

    def _normaliseDetailClass(self, cls: Optional[type] = None) -> Optional[type]:
        if isinstance(cls, type):
            return cls
        resolved = self._resolveClass()
        return resolved if isinstance(resolved, type) else None

    def _getInvalidVars(self, cls: Optional[type] = None) -> Set[str]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return set()
        result: Set[str] = set()
        for base in list(reversed(cls.mro())):
            invalid = getattr(base, "__dict__", {}).get("_invalidVars", ())
            if isinstance(invalid, str):
                result.add(invalid)
            elif isinstance(invalid, (list, tuple, set)):
                result.update(name for name in invalid if isinstance(name, str))
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            invalid = meta.get("InvalidVars", ())
            if isinstance(invalid, str):
                result.add(invalid)
            elif isinstance(invalid, (list, tuple, set)):
                result.update(name for name in invalid if isinstance(name, str))
        return result

    def _getPathVarMap(self, cls: Optional[type] = None) -> Dict[str, str]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return {}
        paths: Dict[str, str] = {}
        for base in list(reversed(cls.mro())):
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            self._collectPathVars(paths, meta.get("PathVars", ()))
        return paths

    def _collectPathVars(self, paths: Dict[str, str], value: Any) -> None:
        if isinstance(value, tuple) and len(value) >= 2 and isinstance(value[0], str):
            paths[value[0]] = self._normalisePathVarAssetsDir(value[1])
            return
        if isinstance(value, (list, tuple, set)):
            for item in value:
                if isinstance(item, str):
                    paths[item] = "Characters"
                    continue
                self._collectPathVars(paths, item)

    def _normalisePathVarAssetsDir(self, value: Any) -> str:
        if not isinstance(value, str):
            return ""
        value = value.replace("\\", "/").strip("/")
        if value in ("", "."):
            return ""
        return value

    def _getRectRangeVars(self, cls: Optional[type] = None) -> Dict[str, str]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return {}
        result: Dict[str, str] = {}
        for base in list(reversed(cls.mro())):
            rects = getattr(base, "__dict__", {}).get("_rectRangeVars", {})
            if isinstance(rects, dict):
                result.update({str(name): str(pathName) for name, pathName in rects.items()})
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            rects = meta.get("RectRangeVars", {})
            if isinstance(rects, dict):
                result.update({str(name): str(pathName) for name, pathName in rects.items()})
        return result

    def _getMetaVarTypes(self, cls: Optional[type] = None) -> Dict[str, str]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return {}
        result: Dict[str, str] = {}
        for base in list(reversed(cls.mro())):
            meta = getattr(base, "__dict__", {}).get("_meta")
            if isinstance(meta, dict):
                result.update(GetMetaVarTypes(meta))
        return result

    def _getGeneralDataVars(self, cls: Optional[type] = None) -> Dict[str, str]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return {}
        result: Dict[str, str] = {}
        for base in list(reversed(cls.mro())):
            meta = getattr(base, "__dict__", {}).get("_meta")
            if isinstance(meta, dict):
                result.update(GetGeneralDataVars(meta))
        return result

    def _getMetaRely(self, cls: Optional[type] = None) -> Dict[str, Any]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return {}
        result: Dict[str, Any] = {}
        for base in list(reversed(cls.mro())):
            meta = getattr(base, "__dict__", {}).get("_meta")
            if isinstance(meta, dict):
                result.update(NormaliseRelyMap(meta.get("Rely")))
        return result

    def _getVariableDisplayMap(self, metaKey: str, cls: Optional[type] = None) -> Dict[str, str]:
        cls = self._normaliseDetailClass(cls)
        if not isinstance(cls, type):
            return {}
        result: Dict[str, str] = {}
        for base in list(reversed(cls.mro())):
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            value = meta.get(metaKey)
            if not isinstance(value, dict):
                continue
            for name, expr in value.items():
                if isinstance(name, str) and isinstance(expr, str):
                    result[name] = self._evalMetaText(expr)
        return result

    def _getVariableDisplayNames(self, cls: Optional[type] = None) -> Dict[str, str]:
        return self._getVariableDisplayMap("VariableDisplayNames", cls)

    def _getVariableDisplayDescs(self, cls: Optional[type] = None) -> Dict[str, str]:
        return self._getVariableDisplayMap("VariableDisplayDescs", cls)

    def _evalMetaText(self, value: str) -> str:
        try:
            return str(eval(value))
        except Exception:
            return value

    def _getClassAttrDisplayName(self, key: str) -> str:
        names = getattr(self, "attrDisplayNames", {})
        if isinstance(names, dict):
            value = names.get(key)
            if isinstance(value, str) and value:
                return value
        return key

    def _getClassAttrDisplayDesc(self, key: str) -> str:
        descs = getattr(self, "attrDisplayDescs", {})
        if isinstance(descs, dict):
            value = descs.get(key)
            if isinstance(value, str):
                return value
        return ""

    def _createClassAttrLabel(self, key: str) -> QtWidgets.QLabel:
        return VariableNameLabel(self._getClassAttrDisplayName(key), key)

    def _reloadClassMetadataFromClass(self, cls: Optional[type] = None) -> None:
        self.invalidVars = self._getInvalidVars(cls)
        self.pathVarMap = self._getPathVarMap(cls)
        self.pathVars = set(self.pathVarMap.keys())
        self.rectRangeVarMap = self._getRectRangeVars(cls)
        self.rectRangeVars = set(self.rectRangeVarMap.keys())
        self.attrVarTypes = self._getMetaVarTypes(cls)
        self.attrGDVars = self._getGeneralDataVars(cls)
        self.attrRely = self._getMetaRely(cls)
        self.attrRelySources = GetRelySourceSet(self.attrRely)
        self.attrDisplayNames = self._getVariableDisplayNames(cls)
        self.attrDisplayDescs = self._getVariableDisplayDescs(cls)

    def _reloadClassMetadata(self, cls: Optional[type] = None) -> None:
        self._reloadClassMetadataFromClass(cls)

    def _copyAttrValue(self, value: Any) -> Any:
        if IsStructuredValue(value):
            return StructuredValueToDict(value)
        try:
            return copy.deepcopy(value)
        except TypeError as e:
            log.warning("Failed to copy class detail attribute value of type %s: %s", type(value).__name__, e)
            return value

    def _getClassDisplayAttrs(self, cls: type) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for attrName in dir(cls):
            if attrName.startswith("_"):
                continue
            try:
                attrVal = getattr(cls, attrName)
            except Exception:
                continue
            if callable(attrVal) or isinstance(attrVal, property):
                continue
            result[attrName] = self._copyAttrValue(attrVal)
        return result

    def _getParentDisplayAttrs(self, parentCls: Optional[type], attrs: Dict[str, Any]) -> Dict[str, Any]:
        if parentCls is None:
            return {}
        result: Dict[str, Any] = {}
        for attrName in dir(parentCls):
            if attrName.startswith("_") or attrName in attrs:
                continue
            try:
                attrVal = getattr(parentCls, attrName)
            except Exception:
                continue
            if callable(attrVal) or isinstance(attrVal, property):
                continue
            result[attrName] = self._copyAttrValue(attrVal)
        return result

    def _getDisplayOrder(self, attrs: Dict[str, Any], cls: Optional[type]) -> list[str]:
        if cls is None or not isinstance(cls, type):
            return list(attrs.keys())

        definedOrder = []
        for base in list(reversed(cls.mro())):
            if base is object:
                continue
            ann = getattr(base, "__annotations__", {})
            for key in ann:
                if key not in definedOrder:
                    definedOrder.append(key)

            for key in getattr(base, "__dict__", {}):
                if key.startswith("_") or key in definedOrder:
                    continue
                try:
                    value = getattr(base, key)
                except Exception:
                    continue
                if callable(value) or isinstance(value, property):
                    continue
                definedOrder.append(key)

        ordered = [key for key in definedOrder if key in attrs]
        seen = set(ordered)
        remaining = [key for key in attrs.keys() if key not in seen]
        return ordered + remaining

    def _setWidgetEditable(self, widget: QtWidgets.QWidget, editable: bool) -> None:
        if isinstance(widget, ColourVarEditor):
            widget.setEditable(editable)
            return
        if isinstance(widget, VectorVarEditor):
            widget.setEditable(editable)
            return
        if isinstance(widget, TypedValueEditor):
            widget.setEditable(editable)
            return
        widget.setEnabled(editable)
        if isinstance(widget, (QtWidgets.QLineEdit, QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
            widget.setReadOnly(not editable)

    def _getRelyTooltip(self, key: str, relyEditable: bool) -> str:
        if relyEditable:
            return ""
        condition = GetRelyConditionDisplay(key, self.attrRely)
        if not condition:
            return ""
        source, value = condition
        return ELOC("META_RELY_TOOLTIP").format(source=source, value=value)

    def _applyRelyTooltip(self, key: str, relyEditable: bool, *widgets: Optional[QtWidgets.QWidget]) -> None:
        parts = []
        desc = self._getClassAttrDisplayDesc(key)
        if desc:
            parts.append(desc)
        relyTip = self._getRelyTooltip(key, relyEditable)
        if relyTip:
            parts.append(relyTip)
        tip = "\n\n".join(parts)
        for widget in widgets:
            if isinstance(widget, GeneralDataComboBox):
                widget.setToolTip(tip or widget.getDefaultToolTip())
            elif widget is not None:
                widget.setToolTip(tip)

    def _isRelyEditable(self, key: str, displayAttrs: Dict[str, Any]) -> bool:
        return IsRelyEditable(key, self.attrRely, displayAttrs)

    def _getComponentTypes(self, cls: Optional[type]) -> Dict[str, type]:
        if cls is None or not isinstance(cls, type):
            return {}
        result: Dict[str, type] = {}
        for base in list(reversed(cls.mro())):
            componentTypes = getattr(base, "__dict__", {}).get("_componentTypes")
            if not isinstance(componentTypes, dict):
                continue
            for name, componentType in componentTypes.items():
                if (
                    isinstance(name, str)
                    and isinstance(componentType, type)
                    and dataclasses.is_dataclass(componentType)
                ):
                    result[name] = componentType
        return result

    def _getComponentFieldMap(self, componentTypes: Dict[str, type]) -> Dict[str, str]:
        result: Dict[str, str] = {}
        for componentName, componentType in componentTypes.items():
            for field in dataclasses.fields(componentType):
                result[field.name] = componentName
        return result

    def _getComponentDefaults(self, componentType: type) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for field in dataclasses.fields(componentType):
            if field.default is not dataclasses.MISSING:
                result[field.name] = copy.deepcopy(field.default)
            elif field.default_factory is not dataclasses.MISSING:
                try:
                    result[field.name] = field.default_factory()
                except Exception as e:
                    log.warning("Failed to create default for %s.%s: %s", componentType.__name__, field.name, e)
        return result

    def _normaliseComponentData(self, componentType: type, value: Any) -> Dict[str, Any]:
        if dataclasses.is_dataclass(value) and not isinstance(value, type):
            value = dataclasses.asdict(value)
        if not isinstance(value, dict):
            value = {}
        result = self._getComponentDefaults(componentType)
        for field in dataclasses.fields(componentType):
            if field.name in value:
                result[field.name] = copy.deepcopy(value[field.name])
        return result

    def _createClassDetailInputWidget(
        self,
        key: str,
        value: Any,
        onChanged: ClassDetailChanged,
        type_hint: Optional[type] = None,
        parent_val: Any = None,
        var_type: str = "",
    ) -> QtWidgets.QWidget:
        if var_type == _GENERALDATA_VAR_TYPE:
            widget = GeneralDataComboBox(_loadGeneralDataMemberKeys(self.attrGDVars.get(key, "")), self)
            widget.setValue(value)
            widget.currentIndexChanged.connect(
                lambda _index, k=key, combo=widget: onChanged(k, combo.getValue(), False)
            )
            return widget
        if var_type == "ColourVar":
            widget = ColourVarEditor(value, self)
            widget.VALUE_CHANGED.connect(lambda val, k=key: onChanged(k, val, False))
            return widget
        if IsVectorVarType(var_type):
            widget = VectorVarEditor(var_type, value, self)
            widget.VALUE_CHANGED.connect(lambda val, k=key: onChanged(k, val, False))
            return widget

        dictType = self._getDictEditorType(value, type_hint, parent_val)
        if dictType is not None:
            widget = TypedValueEditor(value, dictType, self)
            widget.VALUE_CHANGED.connect(lambda val, k=key: onChanged(k, val, False))
            return widget

        if isinstance(value, bool):
            widget = QtWidgets.QCheckBox()
            widget.setChecked(bool(value))
            widget.toggled.connect(lambda checked, k=key: onChanged(k, checked, False))
            return widget

        if type_hint is int or (isinstance(value, int) and not isinstance(value, bool)):
            widget = QtWidgets.QSpinBox()
            widget.setRange(-2147483648, 2147483647)
            try:
                widget.setValue(int(value))
            except (ValueError, TypeError):
                widget.setValue(0)
            widget.valueChanged.connect(lambda val, k=key: onChanged(k, val, False))
            return widget

        if type_hint is float or isinstance(value, float):
            widget = QtWidgets.QDoubleSpinBox()
            widget.setRange(-999999999.0, 999999999.0)
            try:
                widget.setValue(float(value))
            except (ValueError, TypeError):
                widget.setValue(0.0)
            widget.valueChanged.connect(lambda val, k=key: onChanged(k, val, False))
            return widget

        isList = False
        if type_hint:
            origin = getattr(type_hint, "__origin__", None)
            if origin is list:
                isList = True
        elif parent_val is not None and isinstance(parent_val, list):
            isList = True
        elif isinstance(value, list):
            isList = True

        if isList:
            return self._createClassDetailListWidget(key, value, type_hint, onChanged)

        if isinstance(value, tuple):
            container = QtWidgets.QWidget()
            layout = QtWidgets.QHBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)
            edits = []
            for item in value:
                edit = QtWidgets.QLineEdit(str(item))
                layout.addWidget(edit)
                edits.append(edit)
            container._elementWidgets = edits
            container._listIsTuple = True
            for edit in edits:
                edit.textChanged.connect(
                    lambda _, k=key, c=container, cb=onChanged: self._onClassDetailListItemChanged(k, c, cb)
                )
            return container

        widget = QtWidgets.QLineEdit(str(value))
        widget.textChanged.connect(lambda val, k=key: onChanged(k, val, False))
        return widget

    def _createClassDetailListWidget(
        self,
        key: str,
        value: Any,
        type_hint: Optional[type],
        onChanged: ClassDetailChanged,
    ) -> QtWidgets.QWidget:
        container = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        if not isinstance(value, list):
            value = list(value) if isinstance(value, tuple) else []

        edits = []
        for i, item in enumerate(value):
            row = QtWidgets.QWidget()
            rowLayout = QtWidgets.QHBoxLayout(row)
            rowLayout.setContentsMargins(0, 0, 0, 0)
            rowLayout.setSpacing(2)

            edit = QtWidgets.QLineEdit(str(item))
            edit.textChanged.connect(
                lambda _, k=key, c=container, cb=onChanged: self._onClassDetailListItemChanged(k, c, cb)
            )
            rowLayout.addWidget(edit)
            edits.append(edit)

            removeBtn = QtWidgets.QPushButton("-")
            removeBtn.setFixedWidth(24)
            removeBtn.clicked.connect(
                lambda _, idx=i, k=key, c=container, cb=onChanged: self._onRemoveClassDetailListItem(k, c, idx, cb)
            )
            rowLayout.addWidget(removeBtn)

            layout.addWidget(row)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(
            lambda _, k=key, c=container, cb=onChanged: self._onAddClassDetailListItem(k, c, cb)
        )
        layout.addWidget(addBtn)

        container._elementWidgets = edits
        container._listIsTuple = False
        container._originalTypeHint = type_hint
        return container

    def _getDictEditorType(self, value: Any, type_hint: Any, parent_val: Any = None) -> Any | None:
        unwrappedType, _nullable = UnwrapOptionalType(type_hint)
        origin = get_origin(unwrappedType)
        if origin is dict or unwrappedType is dict:
            return type_hint
        if isinstance(parent_val, dict) or isinstance(value, dict):
            return dict
        return None

    def _onRemoveClassDetailListItem(
        self, key: str, container: QtWidgets.QWidget, index: int, onChanged: ClassDetailChanged
    ) -> None:
        elems = getattr(container, "_elementWidgets", [])
        if 0 <= index < len(elems):
            values = []
            for i, edit in enumerate(elems):
                if i == index:
                    continue
                values.append(self._evalTextValue(edit.text()))
            onChanged(key, values, True)

    def _onAddClassDetailListItem(
        self, key: str, container: QtWidgets.QWidget, onChanged: ClassDetailChanged
    ) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = [self._evalTextValue(edit.text()) for edit in elems]

        defaultVal: Any = ""
        typeHint = getattr(container, "_originalTypeHint", None)
        if typeHint:
            args = getattr(typeHint, "__args__", [])
            if args:
                argType = args[0]
                if argType is int:
                    defaultVal = 0
                elif argType is float:
                    defaultVal = 0.0
                elif argType is bool:
                    defaultVal = False
                elif argType is str:
                    defaultVal = ""
        elif values:
            try:
                defaultVal = type(values[-1])()
            except Exception:
                defaultVal = ""

        values.append(defaultVal)
        onChanged(key, values, True)

    def _onClassDetailListItemChanged(
        self, key: str, container: QtWidgets.QWidget, onChanged: ClassDetailChanged
    ) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = [self._evalTextValue(edit.text()) for edit in elems]
        if getattr(container, "_listIsTuple", False):
            onChanged(key, tuple(values), False)
        else:
            onChanged(key, values, False)

    def _evalTextValue(self, text: str) -> Any:
        try:
            return eval(text)
        except Exception:
            return text

    def _getPathVarBaseDir(self, key: str) -> str:
        assetsDir = os.path.join(EditorStatus.PROJ_PATH, "Assets")
        subDir = self.pathVarMap.get(key, "")
        if not subDir:
            return assetsDir
        baseDir = os.path.normpath(os.path.join(assetsDir, subDir))
        try:
            assetsAbs = os.path.normcase(os.path.abspath(assetsDir))
            baseAbs = os.path.normcase(os.path.abspath(baseDir))
            if os.path.commonpath([assetsAbs, baseAbs]) != assetsAbs:
                return assetsDir
        except ValueError:
            return assetsDir
        return baseDir

    def _rectValueToTuple(self, rectValue: Any) -> Optional[tuple[int, int, int, int]]:
        if isinstance(rectValue, (list, tuple)) and len(rectValue) >= 2:
            p0 = rectValue[0]
            p1 = rectValue[1]
            if isinstance(p0, (list, tuple)) and len(p0) >= 2 and isinstance(p1, (list, tuple)) and len(p1) >= 2:
                try:
                    return (int(p0[0]), int(p0[1]), int(p1[0]), int(p1[1]))
                except Exception:
                    return None
        return None
