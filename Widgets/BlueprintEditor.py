# -*- encoding: utf-8 -*-

from __future__ import annotations

import math
import os
import copy
import dataclasses
from typing import Any, Dict, Optional, Set, get_type_hints
from PyQt5 import QtWidgets, QtCore, QtGui
from EditorGlobal import EditorStatus, GameData
from Utils import System, File, SFMLRender
from Widgets.Utils import SingleRowDialog, NodePanel, Toast, RectViewer, DataclassWidget, FileSelectorDialog
from Widgets.Utils.BlueprintPreview import isBlueprintPreviewable
from Widgets.Utils.ColourPickerDialog import ColourVarEditor
from Widgets.Utils.MetaRely import getRelyConditionDisplay, getRelySourceSet, isRelyEditable, normaliseRelyMap
from Widgets.Utils.MetaVarTypes import getMetaVarTypes
from Widgets.Utils.StructuredFields import isStructuredType, isStructuredValue, structuredValueToDict
from Widgets.Utils.VectorVarEditor import VectorVarEditor, isVectorVarType


_GRAPH_TAB_KIND = "graph"
_PREVIEW_TAB_KIND = "preview"


class RevertButton(QtWidgets.QPushButton):
    """A small button with a UE-style curved revert arrow icon."""

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setObjectName("RevertBtn")
        self.setFixedSize(20, 20)
        self.setCursor(QtCore.Qt.PointingHandCursor)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        super().paintEvent(event)
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing)

        enabled = self.isEnabled()
        color = QtGui.QColor("#c0c0c0") if enabled else QtGui.QColor("#585858")
        pen = QtGui.QPen(color, 1.5, QtCore.Qt.SolidLine, QtCore.Qt.RoundCap, QtCore.Qt.RoundJoin)
        p.setPen(pen)
        p.setBrush(QtCore.Qt.NoBrush)

        # Curved arrow body from (14,12) to (7,7), bowing left-bottom
        path = QtGui.QPainterPath()
        path.moveTo(14, 12)
        path.cubicTo(14, 16.5, 3.5, 16, 3.5, 10.5)
        path.cubicTo(3.5, 7.5, 6, 7.5, 7, 7)
        p.drawPath(path)

        # Arrowhead at (7,7) pointing up-left
        p.setBrush(color)
        p.setPen(QtCore.Qt.NoPen)
        arrow = QtGui.QPainterPath()
        arrow.moveTo(7, 7)
        arrow.lineTo(3.5, 4.5)
        arrow.lineTo(10, 4.5)
        arrow.closeSubpath()
        p.drawPath(arrow)


class BluePrintPreviewWidget(QtWidgets.QWidget):
    def __init__(self, editor: "BluePrintEditor") -> None:
        super().__init__(editor)
        self._editor = editor
        self._image: Optional[QtGui.QImage] = None
        self._shaderImage: Optional[QtGui.QImage] = None
        self._rect = (0, 0, 32, 32)
        self._animatable = False
        self._switchInterval = 0.2
        self._shaderTime = 0.0
        self._frame = 0
        self._accumulated = 0.0
        self._clock = QtCore.QElapsedTimer()
        self._timer = QtCore.QTimer(self)
        self._timer.setInterval(50)
        self._timer.timeout.connect(self._onTick)
        self.setMinimumSize(240, 240)
        self.refreshPreview()

    def refreshPreview(self) -> None:
        self._image = self._editor._getPreviewTextureImage()
        self._rect = self._editor._getPreviewRectTuple()
        self._animatable = bool(self._editor._getPreviewAttr("animatable", False))
        try:
            self._switchInterval = max(0.03, float(self._editor._getPreviewAttr("switchInterval", 0.2)))
        except (TypeError, ValueError):
            self._switchInterval = 0.2
        if not self._animatable:
            self._frame = 0
            self._accumulated = 0.0
            self._shaderTime = 0.0
            self._clock.invalidate()
        elif not self._clock.isValid():
            self._clock.start()
        textureWidth = self._image.width() if self._image is not None else 0
        self._shaderImage = self._editor._renderPreviewShaderImage(
            self._rect, self._frame, self._shaderTime, textureWidth
        )
        self._syncTimer()
        self.update()

    def _syncTimer(self) -> None:
        if self._animatable and self._image is not None:
            if not self._timer.isActive():
                self._timer.start()
            return
        if self._timer.isActive():
            self._timer.stop()

    def _onTick(self) -> None:
        if not self._animatable or self._image is None:
            self._syncTimer()
            return
        if not self._clock.isValid():
            self._clock.start()
            return
        elapsed = self._clock.restart() / 1000.0
        self._shaderTime += max(0.0, elapsed)
        self._accumulated += max(0.0, elapsed)
        if self._accumulated < self._switchInterval:
            if self._editor._hasPreviewShader():
                textureWidth = self._image.width() if self._image is not None else 0
                self._shaderImage = self._editor._renderPreviewShaderImage(
                    self._rect, self._frame, self._shaderTime, textureWidth
                )
                self.update()
            return
        while self._accumulated >= self._switchInterval:
            self._frame += 1
            self._accumulated -= self._switchInterval
        textureWidth = self._image.width() if self._image is not None else 0
        self._shaderImage = self._editor._renderPreviewShaderImage(
            self._rect, self._frame, self._shaderTime, textureWidth
        )
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        painter.fillRect(self.rect(), QtGui.QColor(28, 28, 28))
        painter.setRenderHint(QtGui.QPainter.SmoothPixmapTransform, False)
        inner = self.rect().adjusted(24, 24, -24, -24)
        displayImage = self._shaderImage if self._shaderImage is not None else self._image
        if displayImage is None or inner.width() <= 0 or inner.height() <= 0:
            self._drawEmptyPreview(painter, inner)
            return

        sx, sy, sw, sh = self._rect
        sw = max(1, sw)
        sh = max(1, sh)
        if self._shaderImage is not None:
            sx = 0
            sy = 0
            sw = max(1, self._shaderImage.width())
            sh = max(1, self._shaderImage.height())
        elif self._animatable and self._image is not None and self._image.width() > 0:
            sx = (sx + self._frame * sw) % max(1, self._image.width())
        src = QtCore.QRect(sx, sy, sw, sh)
        scale = min(inner.width() / sw, inner.height() / sh)
        scale = max(0.01, scale)
        dstW = max(1, int(sw * scale))
        dstH = max(1, int(sh * scale))
        dst = QtCore.QRect(0, 0, dstW, dstH)
        dst.moveCenter(inner.center())
        painter.drawImage(dst, displayImage, src)
        painter.setPen(QtGui.QPen(QtGui.QColor(90, 90, 90), 1))
        painter.drawRect(dst.adjusted(0, 0, -1, -1))

    def _drawEmptyPreview(self, painter: QtGui.QPainter, rect: QtCore.QRect) -> None:
        size = min(rect.width(), rect.height(), 96)
        if size <= 0:
            return
        box = QtCore.QRect(0, 0, size, size)
        box.moveCenter(rect.center())
        painter.fillRect(box, QtGui.QColor(42, 42, 42))
        painter.setPen(QtGui.QPen(QtGui.QColor(90, 90, 90), 1, QtCore.Qt.DashLine))
        painter.drawRect(box.adjusted(0, 0, -1, -1))
        painter.drawLine(box.topLeft(), box.bottomRight())
        painter.drawLine(box.topRight(), box.bottomLeft())


class BluePrintEditor(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, title: str, data: Dict[str, Any], parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowFlags(QtCore.Qt.Window)
        self.setWindowTitle(title)
        self.setMaximumHeight(600)
        self.resize(1080, 600)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.SetStyle(self, "blueprintEditor.qss")
        self.title = title
        self.data = copy.deepcopy(data)
        self.graphs: Dict[str, Any] = {}
        self.invalidVars: Set[str] = self._getInvalidVars()
        self.pathVarMap: Dict[str, str] = self._getPathVarMap()
        self.pathVars: Set[str] = set(self.pathVarMap.keys())
        rectMap = self._getRectRangeVars()
        self.rectRangeVars: Set[str] = set(rectMap.keys())
        self.rectRangeVarMap: Dict[str, str] = rectMap
        self.attrVarTypes: Dict[str, str] = self._getMetaVarTypes()
        self.attrRely: Dict[str, Any] = self._getMetaRely()
        self.attrRelySources: Set[str] = getRelySourceSet(self.attrRely)
        self.setupUI()
        self.toast = Toast(self)

    def _resolveClass(self) -> Optional[type]:
        if self.title.startswith("__info__/"):
            return None
        key = os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", ".")
        try:
            cls = GameData.classDict.get(key, EditorStatus.PROJ_PATH)
        except (ImportError, Exception):
            return None
        return cls if isinstance(cls, type) else None

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

    def _getWidgetCurrentValue(self, widget: QtWidgets.QWidget) -> Any:
        """Extract the current value from any widget type created by createInputWidget."""
        if isinstance(widget, QtWidgets.QCheckBox):
            return widget.isChecked()
        if isinstance(widget, QtWidgets.QSpinBox):
            return widget.value()
        if isinstance(widget, QtWidgets.QDoubleSpinBox):
            return widget.value()
        if isinstance(widget, QtWidgets.QLineEdit):
            text = widget.text()
            try:
                return eval(text)
            except Exception:
                return text
        if isinstance(widget, DataclassWidget):
            return widget.data
        if isinstance(widget, ColourVarEditor):
            return widget.getValue()
        if isinstance(widget, VectorVarEditor):
            return widget.getValue()
        elems = getattr(widget, "_elementWidgets", None)
        if elems is not None:
            values = []
            for e in elems:
                text = e.text()
                try:
                    values.append(eval(text))
                except Exception:
                    values.append(text)
            if getattr(widget, "_listIsTuple", False):
                return tuple(values)
            return values
        return None

    def _updateRevertButtonState(
        self, btn: RevertButton, current_val: Any, parent_val: Any
    ) -> None:
        """Enable or disable the revert button based on value comparison."""
        equal = GameData._isBlueprintValueEqual(current_val, parent_val)
        btn.setEnabled(not equal)

    def _onRevertAttr(self, key: str, parent_val: Any, widget: QtWidgets.QWidget) -> None:
        """Revert the attribute to its parent class value."""
        if isinstance(widget, ColourVarEditor):
            widget.setValue(parent_val)
            return
        if isinstance(widget, VectorVarEditor):
            widget.setValue(parent_val)
            return

        # For complex widgets where direct set is unreliable, rebuild the form
        is_complex = isinstance(widget, DataclassWidget) or hasattr(widget, "_elementWidgets")
        if is_complex:
            if isinstance(parent_val, (list, tuple)):
                value = copy.deepcopy(list(parent_val))
            elif isStructuredValue(parent_val):
                value = structuredValueToDict(parent_val)
            else:
                value = copy.deepcopy(parent_val)
            self.onDataChanged(key, value, True)
            self.refreshAttrs()
            return

        # Simple widgets: set value directly
        if isinstance(widget, QtWidgets.QCheckBox):
            widget.setChecked(bool(parent_val))
        elif isinstance(widget, QtWidgets.QSpinBox):
            try:
                widget.setValue(int(parent_val))
            except (ValueError, TypeError):
                pass
        elif isinstance(widget, QtWidgets.QDoubleSpinBox):
            try:
                widget.setValue(float(parent_val))
            except (ValueError, TypeError):
                pass
        elif isinstance(widget, QtWidgets.QLineEdit):
            widget.setText(str(parent_val))
        # onDataChanged will be triggered by the widget's own signal

    def _connectRevertUpdate(
        self, widget: QtWidgets.QWidget, revertBtn: RevertButton, parent_val: Any
    ) -> None:
        """Connect widget value-changed signals to keep the revert button state in sync."""
        if isinstance(widget, QtWidgets.QCheckBox):
            widget.toggled.connect(
                lambda checked, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, checked, pv)
            )
        elif isinstance(widget, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
            widget.valueChanged.connect(
                lambda val, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, val, pv)
            )
        elif isinstance(widget, QtWidgets.QLineEdit):
            widget.textChanged.connect(
                lambda text, b=revertBtn, pv=parent_val: self._onRevertTextChanged(b, text, pv)
            )
        elif isinstance(widget, DataclassWidget):
            widget.VALUE_CHANGED.connect(
                lambda data, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, data, pv)
            )
        elif isinstance(widget, ColourVarEditor):
            widget.VALUE_CHANGED.connect(
                lambda data, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, data, pv)
            )
        elif isinstance(widget, VectorVarEditor):
            widget.VALUE_CHANGED.connect(
                lambda data, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, data, pv)
            )
        else:
            elems = getattr(widget, "_elementWidgets", None)
            if elems is not None:
                for e in elems:
                    if isinstance(e, QtWidgets.QLineEdit):
                        e.textChanged.connect(
                            lambda text, b=revertBtn, pv=parent_val, w=widget: self._onContainerRevertChanged(b, w, pv)
                        )

    def _onRevertTextChanged(self, btn: RevertButton, text: str, parent_val: Any) -> None:
        """Handle text changes from QLineEdit to update revert button state."""
        try:
            val = eval(text)
        except Exception:
            val = text
        self._updateRevertButtonState(btn, val, parent_val)

    def _onContainerRevertChanged(self, btn: RevertButton, widget: QtWidgets.QWidget, parent_val: Any) -> None:
        """Handle changes from list/tuple container widgets to update revert button state."""
        val = self._getWidgetCurrentValue(widget)
        self._updateRevertButtonState(btn, val, parent_val)

    def _getInvalidVars(self) -> Set[str]:
        cls = self._resolveClass()
        if cls is None:
            return set()
        result: Set[str] = set()
        try:
            mro = list(reversed(cls.mro()))
        except Exception:
            mro = [cls]
        for base in mro:
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

    def _getPathVarMap(self) -> Dict[str, str]:
        cls = self._resolveClass()
        if cls is None:
            return {}
        paths: Dict[str, str] = {}
        try:
            mro = list(reversed(cls.mro()))
        except Exception:
            mro = [cls]
        for base in mro:
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

    def _getRectRangeVars(self) -> Dict[str, str]:
        cls = self._resolveClass()
        if cls is None:
            return {}
        result: Dict[str, str] = {}
        try:
            mro = list(reversed(cls.mro()))
        except Exception:
            mro = [cls]
        for base in mro:
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

    def _getMetaVarTypes(self) -> Dict[str, str]:
        cls = self._resolveClass()
        if cls is None or not isinstance(cls, type):
            return {}
        result: Dict[str, str] = {}
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]
        for base in mro:
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            result.update(getMetaVarTypes(meta))
        return result

    def _getMetaRely(self) -> Dict[str, Any]:
        cls = self._resolveClass()
        if cls is None or not isinstance(cls, type):
            return {}
        result: Dict[str, Any] = {}
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]
        for base in mro:
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            result.update(normaliseRelyMap(meta.get("Rely")))
        return result

    def _setWidgetEditable(self, widget: QtWidgets.QWidget, editable: bool) -> None:
        if isinstance(widget, ColourVarEditor):
            widget.setEditable(editable)
            return
        if isinstance(widget, VectorVarEditor):
            widget.setEditable(editable)
            return
        widget.setEnabled(editable)
        if isinstance(widget, (QtWidgets.QLineEdit, QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
            widget.setReadOnly(not editable)

    def _getRelyTooltip(self, key: str, relyEditable: bool) -> str:
        if relyEditable:
            return ""
        condition = getRelyConditionDisplay(key, self.attrRely)
        if not condition:
            return ""
        source, value = condition
        return ELOC("META_RELY_TOOLTIP").format(source=source, value=value)

    def _applyRelyTooltip(self, key: str, relyEditable: bool, *widgets: Optional[QtWidgets.QWidget]) -> None:
        tip = self._getRelyTooltip(key, relyEditable)
        for widget in widgets:
            if widget is not None:
                widget.setToolTip(tip)

    def _getComponentTypes(self, cls: Optional[type]) -> Dict[str, Any]:
        if cls is None or not isinstance(cls, type):
            return {}

        result: Dict[str, Any] = {}
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]

        for base in mro:
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

    def _getComponentFieldMap(self, componentTypes: Dict[str, Any]) -> Dict[str, str]:
        result: Dict[str, str] = {}
        for componentName, componentType in componentTypes.items():
            for field in dataclasses.fields(componentType):
                result[field.name] = componentName
        return result

    def _getInfoType(self, cls: Optional[type]) -> str:
        if cls is None or not isinstance(cls, type):
            return ""
        for base in cls.mro():
            getInfoType = getattr(base, "getInfoType", None)
            if callable(getInfoType):
                infoType = getInfoType()
                if isinstance(infoType, str) and infoType:
                    return infoType
        return ""

    def _getGeneralDataMember(self, infoType: str, memberID: Any) -> tuple[Dict[str, Any], Dict[str, Any]]:
        data = GameData.generalData.get(infoType, {})
        if not isinstance(data, dict):
            return {}, {}
        members = data.get("members", {})
        params = data.get("params", {})
        memberData: Dict[str, Any] = {}
        if isinstance(memberID, str) and memberID and isinstance(members, dict):
            rawMember = members.get(memberID, {})
            if isinstance(rawMember, dict):
                memberData = copy.deepcopy(rawMember)
        paramSchema = params if isinstance(params, dict) else {}
        return memberData, paramSchema

    def _getGeneralDataParamKeys(self, infoType: str) -> Set[str]:
        _, params = self._getGeneralDataMember(infoType, "")
        return set(params.keys())

    def _mergeGeneralDataIntoComponent(
        self, cls: Optional[type], componentName: str, componentData: Dict[str, Any], attrs: Dict[str, Any]
    ) -> tuple[Dict[str, Any], Set[str]]:
        infoType = self._getInfoType(cls)
        if not infoType or componentName != "infoComp":
            return componentData, set()
        memberData, params = self._getGeneralDataMember(infoType, attrs.get("ID", ""))
        readOnlyFields = set(params.keys())
        merged = copy.deepcopy(componentData)
        for key in readOnlyFields:
            if key in memberData:
                merged[key] = copy.deepcopy(memberData[key])
        return merged, readOnlyFields

    def _stripGeneralDataFromComponent(
        self, cls: Optional[type], componentName: str, componentData: Dict[str, Any]
    ) -> Dict[str, Any]:
        infoType = self._getInfoType(cls)
        if not infoType or componentName != "infoComp":
            return componentData
        readOnlyFields = self._getGeneralDataParamKeys(infoType)
        result = copy.deepcopy(componentData)
        for key in readOnlyFields:
            result.pop(key, None)
        return result

    def _pruneComponentDataToStored(self, componentType: Any, componentData: Dict[str, Any]) -> Dict[str, Any]:
        defaults = self._getComponentDefaults(componentType)
        result: Dict[str, Any] = {}
        for key, value in componentData.items():
            if key in defaults and value == defaults[key]:
                continue
            result[key] = value
        return result

    def _getComponentDefaults(self, componentType: Any) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for field in dataclasses.fields(componentType):
            if field.default is not dataclasses.MISSING:
                result[field.name] = copy.deepcopy(field.default)
            elif field.default_factory is not dataclasses.MISSING:
                try:
                    result[field.name] = field.default_factory()
                except:
                    pass
        return result

    def _normaliseComponentData(self, componentType: Any, value: Any) -> Dict[str, Any]:
        if dataclasses.is_dataclass(value) and not isinstance(value, type):
            value = dataclasses.asdict(value)
        if not isinstance(value, dict):
            value = {}
        result = self._getComponentDefaults(componentType)
        for field in dataclasses.fields(componentType):
            if field.name in value:
                result[field.name] = copy.deepcopy(value[field.name])
        return result

    def _normaliseComponentAttrs(self, cls: Optional[type], attrs: Dict[str, Any]) -> bool:
        componentTypes = self._getComponentTypes(cls)
        changed = False
        for componentName, componentType in componentTypes.items():
            componentData = self._normaliseComponentData(componentType, attrs.get(componentName))
            moved = False
            skipDisabledLight = False
            if componentType.__name__ == "LightComponent" and "bSelfLight" in attrs:
                enabled = bool(attrs.pop("bSelfLight"))
                moved = True
                if enabled and componentName not in attrs:
                    componentData = self._getComponentDefaults(componentType)
                elif not enabled and componentName not in attrs:
                    skipDisabledLight = True
            for field in dataclasses.fields(componentType):
                if field.name not in attrs:
                    continue
                value = attrs.pop(field.name)
                if not skipDisabledLight:
                    componentData[field.name] = value
                moved = True
            if moved and componentData and not skipDisabledLight:
                if componentName == "infoComp":
                    componentData = self._stripGeneralDataFromComponent(cls, componentName, componentData)
                    componentData = self._pruneComponentDataToStored(componentType, componentData)
                if componentData:
                    attrs[componentName] = componentData
                elif componentName in attrs:
                    del attrs[componentName]
                changed = True
            elif moved:
                changed = True
        return changed

    def _addSeparator(self) -> None:
        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        self.formLayout.addRow(line)

    def _getComponentValue(
        self, cls: Optional[type], componentName: str, componentType: Any, attrs: Dict[str, Any]
    ) -> tuple[Optional[Dict[str, Any]], bool]:
        if componentName in attrs:
            return self._normaliseComponentData(componentType, attrs[componentName]), True
        found, value = self._getClassAttrValue(cls, componentName)
        if found:
            try:
                return self._normaliseComponentData(componentType, value), False
            except:
                pass
        return None, False

    def _getAddableComponents(
        self, cls: Optional[type], componentTypes: Dict[str, Any], attrs: Dict[str, Any]
    ) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for componentName, componentType in componentTypes.items():
            if componentName in attrs:
                continue
            if self._hasClassAttr(cls, componentName):
                continue
            result[componentName] = componentType
        return result

    def _addComponentRows(self, cls: Optional[type], componentTypes: Dict[str, Any], attrs: Dict[str, Any]) -> None:
        items = []
        for componentName, componentType in componentTypes.items():
            value, isLocal = self._getComponentValue(cls, componentName, componentType, attrs)
            if value is None:
                continue
            items.append((componentName, componentType, isLocal))

        addable = self._getAddableComponents(cls, componentTypes, attrs)
        if not items and not addable:
            return

        container = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout(container)
        hbox.setContentsMargins(0, 0, 0, 0)
        hbox.setSpacing(4)

        listWidget = QtWidgets.QListWidget()
        listWidget.setFixedHeight(max(72, min(160, len(items) * 28 + 12)))
        listWidget.itemDoubleClicked.connect(self.onEditComponentItem)
        for componentName, componentType, isLocal in items:
            item = QtWidgets.QListWidgetItem(componentName)
            item.setData(QtCore.Qt.UserRole, componentName)
            item.setData(QtCore.Qt.UserRole + 1, componentType)
            item.setData(QtCore.Qt.UserRole + 2, isLocal)
            listWidget.addItem(item)
        hbox.addWidget(listWidget, 1)

        btnBox = QtWidgets.QWidget()
        btnLayout = QtWidgets.QVBoxLayout(btnBox)
        btnLayout.setContentsMargins(0, 0, 0, 0)
        btnLayout.setSpacing(4)

        addCompBtn = QtWidgets.QPushButton("+")
        addCompBtn.setToolTip(ELOC("ADD_COMPONENT"))
        addCompBtn.setFixedWidth(24)
        addCompBtn.setEnabled(bool(addable))
        addCompBtn.clicked.connect(lambda *_: QtCore.QTimer.singleShot(0, self.onAddComponent))
        btnLayout.addWidget(addCompBtn)

        delCompBtn = QtWidgets.QPushButton("-")
        delCompBtn.setObjectName("MinusBtn")
        delCompBtn.setFixedWidth(24)
        delCompBtn.clicked.connect(lambda _, w=listWidget: self.onDeleteSelectedComponent(w))
        btnLayout.addWidget(delCompBtn)
        btnLayout.addStretch()

        def updateDeleteButton() -> None:
            item = listWidget.currentItem()
            delCompBtn.setEnabled(item is not None and bool(item.data(QtCore.Qt.UserRole + 2)))

        listWidget.currentItemChanged.connect(lambda *_: updateDeleteButton())
        updateDeleteButton()
        hbox.addWidget(btnBox, 0)
        self.formLayout.addRow(QtWidgets.QLabel(ELOC("COMPONENTS")), container)

        if listWidget.count() > 0:
            listWidget.setCurrentRow(0)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        toast = getattr(self, "toast", None)
        if isinstance(toast, Toast):
            toast._updatePosition()
            toast.raise_()

    def setupUI(self) -> None:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        layout.addWidget(self.splitter)

        self.leftPanel = QtWidgets.QWidget()
        self.leftLayout = QtWidgets.QVBoxLayout(self.leftPanel)

        self.formLayout = QtWidgets.QFormLayout()
        self.leftLayout.addLayout(self.formLayout)
        self.leftLayout.addStretch()

        self.leftScroll = QtWidgets.QScrollArea()
        self.leftPanel.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Expanding)
        self.leftPanel.setMinimumWidth(320)
        self.leftScroll.setWidgetResizable(True)
        self.leftScroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.leftScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.leftScroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        self.leftScroll.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.leftScroll.setMinimumWidth(320)
        self.leftScroll.setWidget(self.leftPanel)

        self.rightPanel = QtWidgets.QWidget()
        self.rightLayout = QtWidgets.QVBoxLayout(self.rightPanel)
        self.rightLayout.setSpacing(0)

        self.nodeGraphList = QtWidgets.QListWidget()
        self.nodeGraphList.setFlow(QtWidgets.QListWidget.LeftToRight)
        self.nodeGraphList.setFixedHeight(50)
        self.rightLayout.addWidget(self.nodeGraphList)
        self.nodeGraphList.currentItemChanged.connect(self.onGraphItemSelected)
        self.nodeGraphList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.nodeGraphList.customContextMenuRequested.connect(self.onGraphListContextMenu)
        self._delShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence.Delete, self.nodeGraphList, context=QtCore.Qt.WidgetShortcut
        )
        self._delShortcut.activated.connect(self._onDeleteEvent)
        self._renameShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence(QtCore.Qt.Key_F2), self.nodeGraphList, context=QtCore.Qt.WidgetShortcut
        )
        self._renameShortcut.activated.connect(self._onRenameEvent)

        self.stackedWidget = QtWidgets.QStackedWidget()
        self.rightLayout.addWidget(self.stackedWidget)
        self.previewWidget = BluePrintPreviewWidget(self)
        self.stackedWidget.addWidget(self.previewWidget)

        self.splitter.addWidget(self.leftScroll)
        self.splitter.addWidget(self.rightPanel)
        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 1)
        self.splitter.setSizes([max(320, int(self.width() * 0.4)), max(320, int(self.width() * 0.6))])

        self.refreshAttrs()
        self.refreshGraphList()

    def onGraphItemSelected(
        self, current: Optional[QtWidgets.QListWidgetItem], previous: Optional[QtWidgets.QListWidgetItem]
    ) -> None:
        if current is None:
            return
        if current.data(QtCore.Qt.UserRole) == _PREVIEW_TAB_KIND:
            self._refreshPreview()
            self.stackedWidget.setCurrentWidget(self.previewWidget)
            return
        text = current.text()
        self.onGraphSelected(text)

    def onGraphSelected(self, text: str) -> None:
        if not text:
            return

        if text in self.graphs:
            self.stackedWidget.setCurrentWidget(self.graphs[text])
            return

        self._ensureGraphEvent(text)
        parentCls = self._resolveClass()

        graph = GameData.genGraphFromData(
            self.data["graph"],
            parentCls,
        )
        panel = NodePanel(self, graph, text, self.title, self._refreshData)
        self.graphs[text] = panel
        self.stackedWidget.addWidget(panel)
        self.stackedWidget.setCurrentWidget(panel)

    def _supportsPreview(self) -> bool:
        return isBlueprintPreviewable(self._resolveClass())

    def _collectInheritedGraphKeys(self) -> list[str]:
        result: list[str] = []
        parentPath = self.data.get("parent")
        self._collectBlueprintGraphKeys(parentPath, result, set())
        return result

    def _collectBlueprintGraphKeys(self, classPath: Any, result: list[str], visited: Set[str]) -> None:
        if not isinstance(classPath, str):
            return
        prefix = "Data.Blueprints."
        if not classPath.startswith(prefix):
            return
        key = classPath[len(prefix) :].replace(".", "/")
        if key in visited:
            return
        visited.add(key)
        data = GameData.blueprintsData.get(key)
        if not isinstance(data, dict):
            return
        self._collectBlueprintGraphKeys(data.get("parent"), result, visited)
        graph = data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            return
        for graphKey in nodeGraph.keys():
            if isinstance(graphKey, str) and graphKey not in result:
                result.append(graphKey)

    def _getAvailableGraphKeys(self) -> list[str]:
        result = self._collectInheritedGraphKeys()
        graph = self.data.get("graph")
        if isinstance(graph, dict):
            nodeGraph = graph.get("nodeGraph")
            if isinstance(nodeGraph, dict):
                for key in nodeGraph.keys():
                    if isinstance(key, str) and key not in result:
                        result.append(key)
        return result

    def _ensureGraphEvent(self, name: str) -> None:
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            graph = {}
            self.data["graph"] = graph
        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            nodeGraph = {}
            graph["nodeGraph"] = nodeGraph
        startNodes = graph.get("startNodes")
        if not isinstance(startNodes, dict):
            startNodes = {}
            graph["startNodes"] = startNodes
        if name not in nodeGraph:
            nodeGraph[name] = {"nodes": [], "links": []}
        if name not in startNodes:
            startNodes[name] = None

    def refreshGraphList(self) -> None:
        current_kind = None
        current_graph = None
        current_item = self.nodeGraphList.currentItem()
        if current_item is not None:
            current_kind = current_item.data(QtCore.Qt.UserRole)
            if current_kind == _GRAPH_TAB_KIND:
                current_graph = current_item.text()

        self.nodeGraphList.clear()
        preview_supported = self._supportsPreview()
        if preview_supported:
            previewItem = QtWidgets.QListWidgetItem(ELOC("PREVIEW"))
            previewItem.setData(QtCore.Qt.UserRole, _PREVIEW_TAB_KIND)
            self.nodeGraphList.addItem(previewItem)
        for key in self._getAvailableGraphKeys():
            item = QtWidgets.QListWidgetItem(key)
            item.setData(QtCore.Qt.UserRole, _GRAPH_TAB_KIND)
            self.nodeGraphList.addItem(item)

        if self.nodeGraphList.count() <= 0:
            return
        if preview_supported and current_kind == _PREVIEW_TAB_KIND:
            self.nodeGraphList.setCurrentRow(0)
            return
        if isinstance(current_graph, str) and current_graph:
            graph_item = self._findGraphListItem(current_graph)
            if graph_item is not None:
                self.nodeGraphList.setCurrentItem(graph_item)
                return
        self.nodeGraphList.setCurrentRow(0)

    def onOrganizeGraph(self) -> None:
        item = self.nodeGraphList.currentItem()
        if item is None or self._isPreviewGraphItem(item):
            return
        self.onGraphSelected(item.text())
        currentWidget = self.stackedWidget.currentWidget()
        if not isinstance(currentWidget, NodePanel):
            return
        currentWidget.organizeLayout()

    def _getDisplayOrder(self, attrs: Dict[str, Any], cls: Optional[type]) -> list[str]:
        if not cls or not isinstance(cls, type):
            return list(attrs.keys())

        defined_order = []
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]

        for base in mro:
            if base is object:
                continue

            ann = getattr(base, "__annotations__", {})
            for k in ann:
                if k not in defined_order:
                    defined_order.append(k)

            for k in getattr(base, "__dict__", {}):
                if k.startswith("_"):
                    continue
                if k in defined_order:
                    continue
                try:
                    v = getattr(base, k)
                    if callable(v) or isinstance(v, property):
                        continue
                except:
                    pass
                defined_order.append(k)

        ordered = [k for k in defined_order if k in attrs]
        seen = set(ordered)
        remaining = [k for k in attrs.keys() if k not in seen]
        return ordered + remaining

    def _copyAttrValue(self, value: Any) -> Any:
        if isStructuredValue(value):
            return structuredValueToDict(value)
        try:
            return copy.deepcopy(value)
        except:
            return value

    def _getParentDisplayAttrs(self, parent_cls: Optional[type], attrs: Dict[str, Any]) -> Dict[str, Any]:
        if not parent_cls:
            return {}

        result = {}
        for attr_name in dir(parent_cls):
            if attr_name.startswith("_") or attr_name in attrs:
                continue

            try:
                attr_val = getattr(parent_cls, attr_name)
            except:
                continue

            if callable(attr_val) or isinstance(attr_val, property):
                continue

            result[attr_name] = self._copyAttrValue(attr_val)
        return result

    def refreshAttrs(self) -> None:
        while self.formLayout.rowCount() > 0:
            self.formLayout.removeRow(0)

        cls = self._resolveClass()
        self.attrVarTypes = self._getMetaVarTypes()
        self.attrRely = self._getMetaRely()
        self.attrRelySources = getRelySourceSet(self.attrRely)
        type_hints = {}
        parent_cls = None
        parent_hints = {}
        if cls is not None:
            try:
                type_hints = get_type_hints(cls)
            except:
                type_hints = getattr(cls, "__annotations__", {})

            parent_cls = self._getBaseClass(cls)
            if parent_cls is not None:
                try:
                    parent_hints = get_type_hints(parent_cls)
                except:
                    parent_hints = getattr(parent_cls, "__annotations__", {})

        parent_val = self.data.get("parent", "")
        label = QtWidgets.QLabel(ELOC("PARENT"))
        widget = self.createInputWidget("parent", parent_val, isAttr=False)
        self.formLayout.addRow(label, widget)
        self._addSeparator()

        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            attrs = {}
            self.data["attrs"] = attrs

        target_cls = cls if cls is not None else parent_cls
        componentsChanged = self._normaliseComponentAttrs(target_cls, attrs)
        if componentsChanged:
            GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        componentTypes = self._getComponentTypes(target_cls)
        componentFieldMap = self._getComponentFieldMap(componentTypes)
        componentSkipKeys = set(componentTypes.keys()) | set(componentFieldMap.keys())
        componentParentCls = parent_cls if parent_cls is not None else target_cls
        self._addComponentRows(componentParentCls, componentTypes, attrs)
        self._addSeparator()

        displayAttrs = self._getParentDisplayAttrs(parent_cls, attrs)
        displayAttrs.update(attrs)
        displayAttrs = {k: v for k, v in displayAttrs.items() if k not in componentSkipKeys}
        display_keys = self._getDisplayOrder(displayAttrs, target_cls)

        for key in display_keys:
            value = displayAttrs[key]
            label = QtWidgets.QLabel(str(key))
            container = QtWidgets.QWidget()
            hbox = QtWidgets.QHBoxLayout(container)
            hbox.setContentsMargins(0, 0, 0, 0)
            hbox.setSpacing(4)

            is_dc = False
            type_hint = type_hints.get(key)
            if type_hint is None and key in parent_hints:
                type_hint = parent_hints[key]

            found_attr_parent, attr_parent_val = self._getClassAttrValue(parent_cls, key)
            if not found_attr_parent:
                attr_parent_val = None

            if type_hint and isStructuredType(type_hint):
                widget = DataclassWidget(type_hint, value)
                widget.VALUE_CHANGED.connect(lambda val, k=key: self.onDataChanged(k, val, True))
                is_dc = True
            else:
                widget = self.createInputWidget(
                    key,
                    value,
                    type_hint=type_hint,
                    parent_val=attr_parent_val,
                    var_type=self.attrVarTypes.get(key, ""),
                )

            isInvalid = key in self.invalidVars
            isRectRange = key in self.rectRangeVars and not isInvalid
            isPath = key in self.pathVars and not isInvalid and not isRectRange
            relyEditable = isRelyEditable(key, self.attrRely, displayAttrs)

            if is_dc:
                if isInvalid:
                    widget.setEnabled(False)
            elif isinstance(widget, QtWidgets.QLineEdit):
                if isInvalid or isPath or isRectRange:
                    widget.setReadOnly(True)
                    widget.setStyleSheet("background-color: #303030; color: #aaaaaa;")
                    widget.setCursor(QtCore.Qt.ArrowCursor)
            elif isinstance(widget, QtWidgets.QCheckBox):
                if isInvalid:
                    widget.setEnabled(False)
                    widget.setStyleSheet("color: #aaaaaa;")
            else:
                elems = getattr(widget, "_elementWidgets", None)
                if elems is not None and (isInvalid or isRectRange):
                    for e in elems:
                        if isinstance(e, QtWidgets.QLineEdit):
                            e.setReadOnly(True)
                            e.setStyleSheet("background-color: #303030; color: #aaaaaa;")
                            e.setCursor(QtCore.Qt.ArrowCursor)

            pathBtn = None
            rectBtn = None

            if not relyEditable:
                self._setWidgetEditable(widget, False)

            hbox.addWidget(widget, 1)

            if isPath and isinstance(widget, QtWidgets.QLineEdit):
                pathBtn = QtWidgets.QPushButton("...")
                pathBtn.setObjectName("PathBtn")
                pathBtn.setFixedWidth(24)
                pathBtn.clicked.connect(lambda _, k=key, w=widget: self.onSelectPath(k, w))
                pathBtn.setEnabled(relyEditable)
                hbox.addWidget(pathBtn, 0)

            if isRectRange:
                rectBtn = QtWidgets.QPushButton("...")
                rectBtn.setObjectName("RectBtn")
                rectBtn.setFixedWidth(24)
                rectBtn.clicked.connect(lambda _, k=key: self.onEditRectRange(k))
                rectBtn.setEnabled(relyEditable)
                hbox.addWidget(rectBtn, 0)

            self._applyRelyTooltip(key, relyEditable, label, container, widget, pathBtn, rectBtn)

            has_parent_attr = False
            if parent_cls:
                if self._hasClassAttr(parent_cls, key):
                    has_parent_attr = True
                elif key in parent_hints:
                    has_parent_attr = True

            if not has_parent_attr:
                minusBtn = QtWidgets.QPushButton("-")
                minusBtn.setObjectName("MinusBtn")
                minusBtn.setFixedWidth(24)
                minusBtn.clicked.connect(lambda _, k=key: self.onDeleteAttr(k))
                hbox.addWidget(minusBtn, 0)
            else:
                revertBtn = RevertButton()
                revertBtn.setEnabled(relyEditable)
                if relyEditable:
                    current_val = self._getWidgetCurrentValue(widget)
                    self._updateRevertButtonState(revertBtn, current_val, attr_parent_val)
                revertBtn.clicked.connect(
                    lambda _, k=key, pv=attr_parent_val, w=widget: self._onRevertAttr(k, pv, w)
                )
                self._connectRevertUpdate(widget, revertBtn, attr_parent_val)
                hbox.addWidget(revertBtn, 0)

            self.formLayout.addRow(label, container)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self.onAddAttr)
        self.formLayout.addRow(addBtn)
        self._refreshPreview()

    def createInputWidget(
        self,
        key: str,
        value: Any,
        isAttr: bool = True,
        type_hint: Any = None,
        parent_val: Any = None,
        var_type: Any = "",
    ) -> QtWidgets.QWidget:
        if isAttr and var_type == "ColourVar":
            w = ColourVarEditor(value, self)
            w.VALUE_CHANGED.connect(lambda val, k=key: self.onDataChanged(k, val, True))
            return w
        if isAttr and isVectorVarType(var_type):
            w = VectorVarEditor(var_type, value, self)
            w.VALUE_CHANGED.connect(lambda val, k=key: self.onDataChanged(k, val, True))
            return w

        if isAttr and isinstance(value, bool):
            w = QtWidgets.QCheckBox()
            w.setChecked(bool(value))
            w.toggled.connect(lambda checked, k=key: self.onDataChanged(k, checked, True))
            return w

        if isAttr and (type_hint is int or (isinstance(value, int) and not isinstance(value, bool))):
            w = QtWidgets.QSpinBox()
            w.setRange(-2147483648, 2147483647)
            try:
                w.setValue(int(value))
            except (ValueError, TypeError):
                w.setValue(0)
            w.valueChanged.connect(lambda val, k=key: self.onDataChanged(k, val, True))
            return w

        if isAttr and (type_hint is float or isinstance(value, float)):
            w = QtWidgets.QDoubleSpinBox()
            w.setRange(-999999999.0, 999999999.0)
            try:
                w.setValue(float(value))
            except (ValueError, TypeError):
                w.setValue(0.0)
            w.valueChanged.connect(lambda val, k=key: self.onDataChanged(k, val, True))
            return w

        is_list = False
        if isAttr:
            if type_hint:
                origin = getattr(type_hint, "__origin__", None)
                if origin is list:
                    is_list = True
            elif parent_val is not None and isinstance(parent_val, list):
                is_list = True
            elif isinstance(value, list):
                is_list = True

        if is_list:
            container = QtWidgets.QWidget()
            layout = QtWidgets.QVBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)

            if not isinstance(value, list):
                if isinstance(value, tuple):
                    value = list(value)
                else:
                    value = []

            edits = []
            for i, item in enumerate(value):
                row = QtWidgets.QWidget()
                row_layout = QtWidgets.QHBoxLayout(row)
                row_layout.setContentsMargins(0, 0, 0, 0)
                row_layout.setSpacing(2)

                e = QtWidgets.QLineEdit(str(item))
                e.textChanged.connect(lambda _, k=key, c=container: self._onListItemChanged(k, c))
                row_layout.addWidget(e)
                edits.append(e)

                removeBtn = QtWidgets.QPushButton("-")
                removeBtn.setFixedWidth(24)
                removeBtn.clicked.connect(lambda _, idx=i, k=key, c=container: self._onRemoveListItem(k, c, idx))
                row_layout.addWidget(removeBtn)

                layout.addWidget(row)

            addBtn = QtWidgets.QPushButton("+")
            addBtn.clicked.connect(lambda _, k=key, c=container: self._onAddListItem(k, c))
            layout.addWidget(addBtn)

            container._elementWidgets = edits
            container._listIsTuple = False
            container._originalTypeHint = type_hint
            return container

        if isAttr and isinstance(value, tuple):
            container = QtWidgets.QWidget()
            layout = QtWidgets.QHBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)
            edits = []
            for item in value:
                e = QtWidgets.QLineEdit(str(item))
                layout.addWidget(e)
                edits.append(e)
            container._elementWidgets = edits
            container._listIsTuple = True
            for _e in edits:
                _e.textChanged.connect(lambda _, k=key, c=container: self._onListItemChanged(k, c))
            return container
        w = QtWidgets.QLineEdit(str(value))
        w.textChanged.connect(lambda val, k=key, attr=isAttr: self.onDataChanged(k, val, attr))
        return w

    def _onRemoveListItem(self, key: str, container: QtWidgets.QWidget, index: int) -> None:
        elems = getattr(container, "_elementWidgets", [])
        if 0 <= index < len(elems):
            values = []
            for i, e in enumerate(elems):
                if i == index:
                    continue
                text = e.text()
                try:
                    v = eval(text)
                except:
                    v = text
                values.append(v)

            self.onDataChanged(key, values, True)
            self.refreshAttrs()

    def _onAddListItem(self, key: str, container: QtWidgets.QWidget) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = []
        for e in elems:
            text = e.text()
            try:
                v = eval(text)
            except:
                v = text
            values.append(v)

        default_val = ""
        type_hint = getattr(container, "_originalTypeHint", None)
        if type_hint:
            args = getattr(type_hint, "__args__", [])
            if args:
                arg_type = args[0]
                if arg_type is int:
                    default_val = 0
                elif arg_type is float:
                    default_val = 0.0
                elif arg_type is bool:
                    default_val = False
                elif arg_type is str:
                    default_val = ""
        elif values:
            try:
                default_val = type(values[-1])()
            except:
                pass

        values.append(default_val)
        self.onDataChanged(key, values, True)
        self.refreshAttrs()

    def _onListItemChanged(self, key: str, container: QtWidgets.QWidget) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = []
        for e in elems:
            text = e.text()
            try:
                v = eval(text)
            except:
                v = text
            values.append(v)
        if getattr(container, "_listIsTuple", False):
            self.onDataChanged(key, tuple(values), True)
        else:
            self.onDataChanged(key, values, True)

    def onDataChanged(self, key: str, value: Any, isAttr: bool = True) -> None:
        try:
            value = eval(value)
        except:
            pass
        if isAttr:
            if "attrs" in self.data and isinstance(self.data["attrs"], dict):
                self.data["attrs"][key] = value
        else:
            self.data[key] = value
        GameData.recordSnapshot()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.MODIFIED.emit()
        if not isAttr and key == "parent":
            self.refreshGraphList()
        self._refreshPreview()
        if isAttr and key in self.attrRelySources:
            QtCore.QTimer.singleShot(0, self.refreshAttrs)

    def onDeleteAttr(self, key: str) -> None:
        if "attrs" in self.data and isinstance(self.data["attrs"], dict):
            if key in self.data["attrs"]:
                GameData.recordSnapshot()
                del self.data["attrs"][key]
                self.refreshAttrs()
                GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
                self.MODIFIED.emit()
                self._refreshPreview()

    def onDeleteComponent(self, key: str) -> None:
        self.onDeleteAttr(key)

    def onDeleteSelectedComponent(self, listWidget: QtWidgets.QListWidget) -> None:
        item = listWidget.currentItem()
        if item is None or not bool(item.data(QtCore.Qt.UserRole + 2)):
            return
        key = item.data(QtCore.Qt.UserRole)
        if isinstance(key, str):
            QtCore.QTimer.singleShot(0, lambda k=key: self.onDeleteComponent(k))

    def onEditComponentItem(self, item: QtWidgets.QListWidgetItem) -> None:
        key = item.data(QtCore.Qt.UserRole)
        if isinstance(key, str):
            QtCore.QTimer.singleShot(0, lambda k=key: self.onEditComponent(k))

    def onEditComponent(self, key: str) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            attrs = {}
            self.data["attrs"] = attrs

        cls = self._resolveClass()
        componentTypes = self._getComponentTypes(cls)
        componentType = componentTypes.get(key)
        if componentType is None:
            return

        parent_cls = None
        if cls is not None:
            parent_cls = self._getBaseClass(cls)
        sourceCls = parent_cls if parent_cls is not None else cls
        value, _ = self._getComponentValue(sourceCls, key, componentType, attrs)
        if value is None:
            value = self._getComponentDefaults(componentType)
        displayValue, readOnlyFields = self._mergeGeneralDataIntoComponent(cls, key, value, attrs)

        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle(key)
        layout = QtWidgets.QVBoxLayout(dlg)
        widget = DataclassWidget(componentType, copy.deepcopy(displayValue), dlg, readOnlyFields=readOnlyFields)
        layout.addWidget(widget)
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        ok_btn = buttons.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = buttons.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(ELOC("CONFIRM"))
        if cancel_btn:
            cancel_btn.setText(ELOC("CANCEL"))
        buttons.accepted.connect(dlg.accept)
        buttons.rejected.connect(dlg.reject)
        layout.addWidget(buttons)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return

        GameData.recordSnapshot()
        saved = self._stripGeneralDataFromComponent(cls, key, copy.deepcopy(widget.data))
        saved = self._pruneComponentDataToStored(componentType, saved)
        if saved:
            attrs[key] = saved
        elif key in attrs:
            del attrs[key]
        self.refreshAttrs()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.MODIFIED.emit()

    def onAddComponent(self) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            attrs = {}
            self.data["attrs"] = attrs

        cls = self._resolveClass()
        componentTypes = self._getComponentTypes(cls)
        parent_cls = None
        if cls is not None:
            parent_cls = self._getBaseClass(cls)
        addable = self._getAddableComponents(parent_cls, componentTypes, attrs)
        if not addable:
            return

        displayItems = [f"{name} ({componentType.__name__})" for name, componentType in addable.items()]
        selected, ok = QtWidgets.QInputDialog.getItem(
            self,
            ELOC("ADD_COMPONENT"),
            ELOC("COMPONENT_NAME"),
            displayItems,
            0,
            False,
        )
        if not ok or not selected:
            return

        componentName = selected.split(" ", 1)[0]
        componentType = addable.get(componentName)
        if componentType is None:
            return

        GameData.recordSnapshot()
        attrs[componentName] = self._getComponentDefaults(componentType)
        self.refreshAttrs()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.MODIFIED.emit()

    def onAddAttr(self) -> None:
        dlg = SingleRowDialog(self, ELOC("ADD_ATTR"), ELOC("ATTR_NAME"), "", None)
        ok, key = dlg.execGetText()
        if ok:
            key = key.strip()
            if not key:
                return

            if key[0].isdigit():
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_NAME_CANNOT_START_WITH_DIGIT"))
                return

            if key in self.invalidVars:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
                return

            if "attrs" not in self.data or not isinstance(self.data["attrs"], dict):
                self.data["attrs"] = {}
            cls = self._resolveClass()
            componentTypes = self._getComponentTypes(cls)
            componentFieldMap = self._getComponentFieldMap(componentTypes)
            if key in componentTypes or key in componentFieldMap:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
                return
            if key in self.data["attrs"]:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_EXISTS"))
                return

            GameData.recordSnapshot()
            self.data["attrs"][key] = ""
            self.refreshAttrs()
            GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
            self.MODIFIED.emit()

    def onSelectPath(self, key: str, widget: QtWidgets.QLineEdit) -> None:
        baseDir = self._getPathVarBaseDir(key)
        if not os.path.isdir(baseDir):
            assetsDir = os.path.join(EditorStatus.PROJ_PATH, "Assets")
            baseDir = assetsDir if os.path.isdir(assetsDir) else EditorStatus.PROJ_PATH
        dlg = FileSelectorDialog(self, baseDir, FileSelectorDialog.allFilesFilter(star=True))
        filePath = dlg.execSelect()
        if not filePath:
            return
        try:
            relPath = os.path.relpath(filePath, baseDir)
        except ValueError:
            relPath = filePath
        relPath = relPath.replace("\\", "/")
        widget.setText(relPath)

    def onEditRectRange(self, key: str) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            return
        pathKey = self.rectRangeVarMap.get(key)
        if not pathKey:
            return
        pathValue = attrs.get(pathKey)
        if not isinstance(pathValue, str) or not pathValue:
            cls = self._resolveClass()
            if cls is not None:
                found, parentVal = self._getClassAttrValue(cls, pathKey)
                if found and isinstance(parentVal, str) and parentVal:
                    pathValue = parentVal
        if not isinstance(pathValue, str) or not pathValue:
            return
        baseDir = self._getPathVarBaseDir(pathKey)
        imagePath = os.path.join(baseDir, pathValue)
        rectValue = attrs.get(key)
        rectTuple = None
        if isinstance(rectValue, (list, tuple)) and len(rectValue) >= 2:
            p0 = rectValue[0]
            p1 = rectValue[1]
            if isinstance(p0, (list, tuple)) and len(p0) >= 2 and isinstance(p1, (list, tuple)) and len(p1) >= 2:
                try:
                    x = int(p0[0])
                    y = int(p0[1])
                    w = int(p1[0])
                    h = int(p1[1])
                    rectTuple = (x, y, w, h)
                except Exception:
                    rectTuple = None
        if rectTuple is None:
            cell = getattr(EditorStatus, "CELLSIZE", 0)
            if not isinstance(cell, int) or cell <= 0:
                cell = 32
            rectTuple = (0, 0, cell, cell)
        dlg = RectViewer(self, imagePath, rectTuple)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        nx, ny, nw, nh = dlg.getRectTuple()
        newValue = ((nx, ny), (nw, nh))
        self.onDataChanged(key, newValue, True)
        self.refreshAttrs()

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

    def _refreshPreview(self) -> None:
        if not self._supportsPreview():
            return
        preview = getattr(self, "previewWidget", None)
        if isinstance(preview, BluePrintPreviewWidget):
            preview.refreshPreview()

    def _getPreviewAttr(self, key: str, default: Any = None) -> Any:
        attrs = self.data.get("attrs")
        if isinstance(attrs, dict) and key in attrs:
            return attrs.get(key, default)
        found, value = self._getClassAttrValue(self._resolveClass(), key)
        return value if found else default

    def _getPreviewTextureImage(self) -> Optional[QtGui.QImage]:
        path = self._getPreviewTexturePath()
        if not path:
            return None
        img = QtGui.QImage(path)
        if img.isNull():
            return None
        return img

    def _getPreviewTexturePath(self) -> str:
        texturePath = self._getPreviewAttr("texturePath", "")
        if not isinstance(texturePath, str) or not texturePath.strip():
            return ""
        p = texturePath.strip()
        if os.path.isabs(p):
            return p
        if p.startswith("Assets/") or p.startswith("Assets\\"):
            return os.path.join(EditorStatus.PROJ_PATH, p)
        return os.path.join(self._getPathVarBaseDir("texturePath"), p)

    def _getPreviewShaderPath(self) -> str:
        shaderPath = self._getPreviewAttr("shaderPath", "")
        if not isinstance(shaderPath, str) or not shaderPath.strip():
            return ""
        p = shaderPath.strip()
        if os.path.isabs(p):
            return p
        if p.startswith("Assets/Shaders/") or p.startswith("Assets\\Shaders\\"):
            return os.path.join(EditorStatus.PROJ_PATH, p)
        return os.path.join(self._getPathVarBaseDir("shaderPath"), p)

    def _hasPreviewShader(self) -> bool:
        path = self._getPreviewShaderPath()
        return SFMLRender.hasUsableShader(path)

    def _renderPreviewShaderImage(
        self, rect: tuple[int, int, int, int], frame: int, shaderTime: float, textureWidth: int = 0
    ) -> Optional[QtGui.QImage]:
        texturePath = self._getPreviewTexturePath()
        shaderPath = self._getPreviewShaderPath()
        encoded = SFMLRender.renderTextureWithShaderToMemory(
            texturePath,
            shaderPath,
            rect,
            frame=frame,
            shaderTime=shaderTime,
            textureWidth=textureWidth,
        )
        if encoded is None:
            return None
        renderedImage = QtGui.QImage()
        if not renderedImage.loadFromData(encoded, "PNG"):
            return None
        return renderedImage

    def _getPreviewRectTuple(self) -> tuple[int, int, int, int]:
        rect = self._getPreviewAttr("defaultRect", None)
        if isinstance(rect, (list, tuple)) and len(rect) >= 2:
            pos = rect[0]
            size = rect[1]
            if isinstance(pos, (list, tuple)) and isinstance(size, (list, tuple)) and len(pos) >= 2 and len(size) >= 2:
                try:
                    x = int(pos[0])
                    y = int(pos[1])
                    w = max(1, int(size[0]))
                    h = max(1, int(size[1]))
                    return (x, y, w, h)
                except (TypeError, ValueError):
                    pass
        cell = getattr(EditorStatus, "CELLSIZE", 32)
        try:
            cell = max(1, int(cell))
        except (TypeError, ValueError):
            cell = 32
        return (0, 0, cell, cell)

    def _refreshListFromData(self) -> None:
        if self.title not in GameData.blueprintsData:
            return
        self.data = copy.deepcopy(GameData.blueprintsData[self.title])
        self.refreshAttrs()
        current_row = self.nodeGraphList.currentRow()
        self.refreshGraphList()
        if current_row >= 0 and current_row < self.nodeGraphList.count():
            self.nodeGraphList.setCurrentRow(current_row)

    def _refreshCurrentPanel(self) -> None:
        current_widget = self.stackedWidget.currentWidget()
        if isinstance(current_widget, NodePanel):
            graph_key = current_widget.name
            self._ensureGraphEvent(graph_key)
            parentCls = self._resolveClass()
            graph = GameData.genGraphFromData(
                self.data["graph"],
                parentCls,
            )
            current_widget.nodeGraph = graph
            current_widget._refreshPanel()

    def _onUndo(self) -> None:
        diffs = GameData.undo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.GetTitle())
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self) -> None:
        diffs = GameData.redo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.GetTitle())
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _refreshData(self, name: str, data: Dict[str, Any]) -> None:
        GameData.recordSnapshot()
        if name in GameData.blueprintsData:
            GameData.blueprintsData[name]["graph"] = data
        self.data["graph"] = data
        self.MODIFIED.emit()

    def _isPreviewGraphItem(self, item: Optional[QtWidgets.QListWidgetItem]) -> bool:
        return item is not None and item.data(QtCore.Qt.UserRole) == _PREVIEW_TAB_KIND

    def _findGraphListItem(self, name: str) -> Optional[QtWidgets.QListWidgetItem]:
        for i in range(self.nodeGraphList.count()):
            item = self.nodeGraphList.item(i)
            if item is not None and item.data(QtCore.Qt.UserRole) == _GRAPH_TAB_KIND and item.text() == name:
                return item
        return None

    def onGraphListContextMenu(self, pos: QtCore.QPoint) -> None:
        index = self.nodeGraphList.indexAt(pos)
        has_item = index.isValid()
        if has_item:
            self.nodeGraphList.setCurrentRow(index.row())
        menu = QtWidgets.QMenu(self)
        action_new = menu.addAction(ELOC("NEW_EVENT"))
        if action_new is None:
            return
        action_new.triggered.connect(self._onNewEvent)
        item = self.nodeGraphList.currentItem() if has_item else None
        if has_item and not self._isPreviewGraphItem(item):
            action_organise = menu.addAction(ELOC("ORGANIZE_GRAPH"))
            action_rename = menu.addAction(ELOC("RENAME_EVENT"))
            action_del = menu.addAction(ELOC("DELETE_EVENT"))

            if action_organise is None or action_rename is None or action_del is None:
                return

            action_organise.setToolTip(ELOC("ORGANIZE_GRAPH_TIP"))
            action_organise.triggered.connect(self.onOrganizeGraph)
            action_rename.triggered.connect(self._onRenameEvent)
            action_del.triggered.connect(self._onDeleteEvent)
        menu.exec_(self.nodeGraphList.mapToGlobal(pos))

    def _onNewEvent(self) -> None:
        dlg = SingleRowDialog(self, ELOC("NEW_EVENT"), ELOC("ENTER_EVENT_NAME"), "", None)
        ok, name = dlg.execGetText()
        if not ok:
            return
        name = name.strip()
        if not name:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
            return
        if name[0].isdigit():
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_NAME_CANNOT_START_WITH_DIGIT"))
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            graph = {}
            self.data["graph"] = graph
        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            nodeGraph = {}
            graph["nodeGraph"] = nodeGraph
        startNodes = graph.get("startNodes")
        if not isinstance(startNodes, dict):
            startNodes = {}
            graph["startNodes"] = startNodes
        if name in self._getAvailableGraphKeys():
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("EVENT_EXISTS"))
            return
        GameData.recordSnapshot()
        nodeGraph[name] = {"nodes": [], "links": []}
        startNodes[name] = None
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        item = self._findGraphListItem(name)
        if item is not None:
            self.nodeGraphList.setCurrentItem(item)
        self.MODIFIED.emit()

    def _onDeleteEvent(self) -> None:
        item = self.nodeGraphList.currentItem()
        if not item or self._isPreviewGraphItem(item):
            return
        name = item.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("CONFIRM_DELETE_EVENT").format(name=name),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        startNodes = graph.get("startNodes")
        GameData.recordSnapshot()
        if isinstance(nodeGraph, dict) and name in nodeGraph:
            del nodeGraph[name]
        if isinstance(startNodes, dict) and name in startNodes:
            del startNodes[name]
        if name in self.graphs:
            panel = self.graphs.pop(name)
            self.stackedWidget.removeWidget(panel)
            panel.deleteLater()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        if self.nodeGraphList.count() > 0:
            self.nodeGraphList.setCurrentRow(0)
        self.MODIFIED.emit()

    def _onRenameEvent(self) -> None:
        item = self.nodeGraphList.currentItem()
        if not item or self._isPreviewGraphItem(item):
            return
        old_name = item.text()
        dlg = SingleRowDialog(self, ELOC("RENAME_EVENT"), ELOC("ENTER_EVENT_NAME"), old_name, None)
        ok, new_name = dlg.execGetText()
        if not ok:
            return
        new_name = new_name.strip()
        if not new_name or new_name == old_name:
            return
        if new_name[0].isdigit():
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_NAME_CANNOT_START_WITH_DIGIT"))
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        startNodes = graph.get("startNodes")
        if not isinstance(nodeGraph, dict) or not isinstance(startNodes, dict):
            return
        if new_name in nodeGraph:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("EVENT_EXISTS"))
            return
        GameData.recordSnapshot()
        new_nodeGraph = {}
        for k, v in nodeGraph.items():
            if k == old_name:
                new_nodeGraph[new_name] = v
            else:
                new_nodeGraph[k] = v
        graph["nodeGraph"] = new_nodeGraph
        new_startNodes = {}
        for k, v in startNodes.items():
            if k == old_name:
                new_startNodes[new_name] = v
            else:
                new_startNodes[k] = v
        graph["startNodes"] = new_startNodes
        if old_name in self.graphs:
            panel = self.graphs.pop(old_name)
            panel.setName(new_name)
            self.graphs[new_name] = panel
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        item = self._findGraphListItem(new_name)
        if item is not None:
            self.nodeGraphList.setCurrentItem(item)
        self.MODIFIED.emit()
