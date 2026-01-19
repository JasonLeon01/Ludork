# -*- encoding: utf-8 -*-
from typing import Optional, Dict, Any, Tuple
from PyQt5 import QtWidgets, QtCore, QtGui
from Utils import Locale, Panel
from Data import GameData


class ActorInfoPanel(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._layerName: Optional[str] = None
        self._index: Optional[int] = None
        self._editorPanel = None
        self._blockSignals = False

        self.setStyleSheet("")

        self.mainLayout = QtWidgets.QVBoxLayout(self)
        self.mainLayout.setContentsMargins(4, 4, 4, 4)
        self.mainLayout.setSpacing(4)

        # Title
        self.titleLabel = QtWidgets.QLabel(Locale.getContent("ACTOR_INFO"))
        self.titleLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.titleLabel.setStyleSheet("font-weight: bold; background-color: #444; padding: 4px; border-radius: 4px;")
        self.mainLayout.addWidget(self.titleLabel)

        # Content
        self.formLayout = QtWidgets.QFormLayout()
        self.formLayout.setContentsMargins(0, 0, 0, 0)
        self.formLayout.setSpacing(4)

        # Tag
        self.tagEdit = QtWidgets.QLineEdit()
        self.tagEdit.textChanged.connect(self._onTagChanged)
        self.formLayout.addRow(Locale.getContent("TAG"), self.tagEdit)

        # Translation
        self.translationXSpin = QtWidgets.QDoubleSpinBox()
        self.translationXSpin.setRange(-10000.0, 10000.0)
        self.translationXSpin.setSingleStep(1.0)
        self.translationXSpin.valueChanged.connect(self._onTranslationChanged)

        self.translationYSpin = QtWidgets.QDoubleSpinBox()
        self.translationYSpin.setRange(-10000.0, 10000.0)
        self.translationYSpin.setSingleStep(1.0)
        self.translationYSpin.valueChanged.connect(self._onTranslationChanged)

        translationLayout = QtWidgets.QHBoxLayout()
        translationLayout.addWidget(QtWidgets.QLabel("X:"))
        translationLayout.addWidget(self.translationXSpin)
        translationLayout.addWidget(QtWidgets.QLabel("Y:"))
        translationLayout.addWidget(self.translationYSpin)
        self.formLayout.addRow(Locale.getContent("TRANSLATION"), translationLayout)

        # Rotation
        self.rotationSpin = QtWidgets.QDoubleSpinBox()
        self.rotationSpin.setRange(-360.0, 360.0)
        self.rotationSpin.setSingleStep(15.0)
        self.rotationSpin.valueChanged.connect(self._onRotationChanged)
        self.formLayout.addRow(Locale.getContent("ROTATION"), self.rotationSpin)

        # Scale
        self.scaleXSpin = QtWidgets.QDoubleSpinBox()
        self.scaleXSpin.setRange(-100.0, 100.0)
        self.scaleXSpin.setSingleStep(0.1)
        self.scaleXSpin.valueChanged.connect(self._onScaleChanged)

        self.scaleYSpin = QtWidgets.QDoubleSpinBox()
        self.scaleYSpin.setRange(-100.0, 100.0)
        self.scaleYSpin.setSingleStep(0.1)
        self.scaleYSpin.valueChanged.connect(self._onScaleChanged)

        scaleLayout = QtWidgets.QHBoxLayout()
        scaleLayout.addWidget(QtWidgets.QLabel("X:"))
        scaleLayout.addWidget(self.scaleXSpin)
        scaleLayout.addWidget(QtWidgets.QLabel("Y:"))
        scaleLayout.addWidget(self.scaleYSpin)
        self.formLayout.addRow(Locale.getContent("SCALE"), scaleLayout)

        # Origin
        self.originXSpin = QtWidgets.QDoubleSpinBox()
        self.originXSpin.setRange(-10000.0, 10000.0)
        self.originXSpin.setSingleStep(1.0)
        self.originXSpin.valueChanged.connect(self._onOriginChanged)

        self.originYSpin = QtWidgets.QDoubleSpinBox()
        self.originYSpin.setRange(-10000.0, 10000.0)
        self.originYSpin.setSingleStep(1.0)
        self.originYSpin.valueChanged.connect(self._onOriginChanged)

        originLayout = QtWidgets.QHBoxLayout()
        originLayout.addWidget(QtWidgets.QLabel("X:"))
        originLayout.addWidget(self.originXSpin)
        originLayout.addWidget(QtWidgets.QLabel("Y:"))
        originLayout.addWidget(self.originYSpin)
        self.formLayout.addRow(Locale.getContent("ORIGIN"), originLayout)

        self.mainLayout.addLayout(self.formLayout)

        self.noSelectionLabel = QtWidgets.QLabel(Locale.getContent("NO_SELECTION"))
        self.noSelectionLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.noSelectionLabel.setStyleSheet("color: #888; font-style: italic; margin-top: 20px;")
        self.mainLayout.addWidget(self.noSelectionLabel)

        self.mainLayout.addStretch()

        self.setEnabled(False)
        self.noSelectionLabel.setVisible(True)
        self.titleLabel.setVisible(False)
        for i in range(self.formLayout.count()):
            w = self.formLayout.itemAt(i).widget()
            if w:
                w.setVisible(False)
            l = self.formLayout.itemAt(i).layout()
            if l:
                for j in range(l.count()):
                    lw = l.itemAt(j).widget()
                    if lw:
                        lw.setVisible(False)

    def setActor(
        self, layerName: Optional[str], index: Optional[int], data: Optional[Dict[str, Any]], editorPanel
    ) -> None:
        self._editorPanel = editorPanel
        self._layerName = layerName
        self._index = index

        if layerName is None or index is None or data is None:
            self.setEnabled(False)
            self.noSelectionLabel.setVisible(True)
            self.titleLabel.setVisible(False)
            self._setFormVisible(False)
            return

        self.setEnabled(True)
        self.noSelectionLabel.setVisible(False)
        self.titleLabel.setVisible(True)
        self._setFormVisible(True)

        self._blockSignals = True
        try:
            self.tagEdit.setText(str(data.get("tag", "")))

            bp = data.get("bp")

            defTransData = self._getBlueprintAttr(bp, "defaultTranslation", (0.0, 0.0))
            defTrans = self._toVec2f(defTransData, 0.0, 0.0)
            curTrans = self._toVec2f(data.get("translation", defTrans), defTrans[0], defTrans[1])
            self.translationXSpin.setValue(curTrans[0])
            self.translationYSpin.setValue(curTrans[1])

            defRot = self._getBlueprintAttr(bp, "defaultRotation", 0.0)
            curRot = data.get("rotation", defRot)
            self.rotationSpin.setValue(float(curRot))

            defScaleData = self._getBlueprintAttr(bp, "defaultScale", (1.0, 1.0))
            defScale = self._toVec2f(defScaleData, 1.0, 1.0)
            curScale = self._toVec2f(data.get("scale", defScale), defScale[0], defScale[1])
            self.scaleXSpin.setValue(curScale[0])
            self.scaleYSpin.setValue(curScale[1])

            defOriginData = self._getBlueprintAttr(bp, "defaultOrigin", (0.0, 0.0))
            defOrigin = self._toVec2f(defOriginData, 0.0, 0.0)
            curOrigin = self._toVec2f(data.get("origin", defOrigin), defOrigin[0], defOrigin[1])
            self.originXSpin.setValue(curOrigin[0])
            self.originYSpin.setValue(curOrigin[1])

        except Exception as e:
            print(f"Error setting actor info: {e}")

        self._blockSignals = False

    def _setFormVisible(self, visible: bool):
        for i in range(self.formLayout.count()):
            item = self.formLayout.itemAt(i)
            w = item.widget()
            if w:
                w.setVisible(visible)
            l = item.layout()
            if l:
                for j in range(l.count()):
                    lw = l.itemAt(j).widget()
                    if lw:
                        lw.setVisible(visible)

    def _getActorData(self) -> Optional[Dict[str, Any]]:
        if not self._editorPanel or not self._layerName or self._index is None:
            return None
        m = GameData.mapData.get(self._editorPanel.mapKey)
        if not isinstance(m, dict):
            return None
        actors = m.get("actors", {}).get(self._layerName)
        if not isinstance(actors, list) or not (0 <= self._index < len(actors)):
            return None
        return actors[self._index]

    def _updateData(self, key: str, value: Any, defaultVal: Any) -> None:
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return

        GameData.recordSnapshot()

        # Check if value equals default
        isDefault = False
        if isinstance(value, float) and isinstance(defaultVal, float):
            isDefault = abs(value - defaultVal) < 0.0001
        elif isinstance(value, (list, tuple)) and isinstance(defaultVal, (list, tuple)):
            if len(value) == len(defaultVal):
                isDefault = True
                for i in range(len(value)):
                    if abs(value[i] - defaultVal[i]) >= 0.0001:
                        isDefault = False
                        break
        elif value == defaultVal:
            isDefault = True

        if isDefault:
            if key in data:
                del data[key]
        else:
            data[key] = value

        if self._editorPanel:
            self._editorPanel._refreshTitle()
            self._editorPanel.dataChanged.emit()
            self._editorPanel._renderFromMapData()
            self._editorPanel.update()

    def _onTagChanged(self, text: str):
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return
        GameData.recordSnapshot()
        data["tag"] = text
        if self._editorPanel:
            self._editorPanel.dataChanged.emit()

    def _onTranslationChanged(self):
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return

        bp = data.get("bp")
        defTransData = self._getBlueprintAttr(bp, "defaultTranslation", (0.0, 0.0))
        defTrans = self._toVec2f(defTransData, 0.0, 0.0)

        val = [self.translationXSpin.value(), self.translationYSpin.value()]
        self._updateData("translation", val, list(defTrans))

    def _onRotationChanged(self, val: float):
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return

        bp = data.get("bp")
        defRot = self._getBlueprintAttr(bp, "defaultRotation", 0.0)

        self._updateData("rotation", val, float(defRot))

    def _onScaleChanged(self):
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return

        bp = data.get("bp")
        defScaleData = self._getBlueprintAttr(bp, "defaultScale", (1.0, 1.0))
        defScale = self._toVec2f(defScaleData, 1.0, 1.0)

        val = [self.scaleXSpin.value(), self.scaleYSpin.value()]
        self._updateData("scale", val, list(defScale))

    def _onOriginChanged(self):
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return

        bp = data.get("bp")
        defOriginData = self._getBlueprintAttr(bp, "defaultOrigin", (0.0, 0.0))
        defOrigin = self._toVec2f(defOriginData, 0.0, 0.0)

        val = [self.originXSpin.value(), self.originYSpin.value()]
        self._updateData("origin", val, list(defOrigin))

    # Helpers copied/adapted from EditorPanel
    def _getClassAttr(self, cls: Any, name: str, default: Any) -> Any:
        if hasattr(cls, name):
            return getattr(cls, name)
        return default

    def _getBlueprintAttr(self, bpRel: Any, name: str, default: Any) -> Any:
        if self._editorPanel and hasattr(self._editorPanel, "getBlueprintAttr"):
            return self._editorPanel.getBlueprintAttr(bpRel, name, default)
        clsObj = self._editorPanel._resolveActorClass(bpRel) if self._editorPanel else None
        return self._getClassAttr(clsObj, name, default)

    def _toVec2f(self, data: Any, defaultX: float = 0.0, defaultY: float = 0.0) -> Tuple[float, float]:
        if isinstance(data, (list, tuple)) and len(data) >= 2:
            try:
                return (float(data[0]), float(data[1]))
            except Exception:
                return (defaultX, defaultY)
        return (defaultX, defaultY)
