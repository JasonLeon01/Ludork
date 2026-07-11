# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import builtins
import os
import sys
from typing import Any

from PyQt5 import QtCore, QtGui, QtQuickWidgets, QtWidgets

from EditorGlobal.QmlDialogHost import buildQmlTheme, QmlDialogHost

from .DialogUtils import GetIndependentDialogParent, IsWidgetValid


def _loc(key: str) -> str:
    eloc = getattr(builtins, "ELOC", None)
    if not callable(eloc):
        return key
    try:
        value = eloc(key)
    except Exception:
        return key
    return value if isinstance(value, str) else key


def _clampChannel(value: Any, default: int = 0) -> int:
    try:
        return max(0, min(255, int(value)))
    except (TypeError, ValueError):
        return default


def _parseHexColourText(text: str, defaultAlpha: int = 255) -> tuple[int, int, int, int] | None:
    raw = text.strip()
    if raw.startswith("#"):
        raw = raw[1:]
    if len(raw) not in (6, 8):
        return None
    try:
        r = int(raw[0:2], 16)
        g = int(raw[2:4], 16)
        b = int(raw[4:6], 16)
        a = int(raw[6:8], 16) if len(raw) == 8 else defaultAlpha
    except ValueError:
        return None
    return r, g, b, a


def _parseHexColour(text: str) -> tuple[int, int, int, int] | None:
    if not text.strip().startswith("#"):
        return None
    return _parseHexColourText(text)


def _formatHexColour(colour: QtGui.QColor) -> str:
    if colour.alpha() == 255:
        return "#{:02X}{:02X}{:02X}".format(colour.red(), colour.green(), colour.blue())
    return "#{:02X}{:02X}{:02X}{:02X}".format(
        colour.red(), colour.green(), colour.blue(), colour.alpha()
    )


def ColourTupleFromValue(value: Any) -> tuple[int, int, int, int]:
    if isinstance(value, QtGui.QColor):
        return value.red(), value.green(), value.blue(), value.alpha()
    if isinstance(value, str):
        text = value.strip()
        parsedHex = _parseHexColour(text)
        if parsedHex is not None:
            return parsedHex
        try:
            value = ast.literal_eval(text)
        except (ValueError, SyntaxError):
            return 255, 255, 255, 255
    valueType = type(value)
    if valueType.__name__ == "Color" and valueType.__module__.startswith("pysf"):
        return (
            _clampChannel(value.r),
            _clampChannel(value.g),
            _clampChannel(value.b),
            _clampChannel(value.a, 255),
        )
    if isinstance(value, (list, tuple)):
        channels = list(value[:4])
        while len(channels) < 4:
            channels.append(255 if len(channels) == 3 else 0)
        return (
            _clampChannel(channels[0]),
            _clampChannel(channels[1]),
            _clampChannel(channels[2]),
            _clampChannel(channels[3], 255),
        )
    return 255, 255, 255, 255


def QColorFromValue(value: Any) -> QtGui.QColor:
    return QtGui.QColor(*ColourTupleFromValue(value))


class _ScreenColourOverlay(QtWidgets.QWidget):
    colourPicked = QtCore.pyqtSignal(object)

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(
            parent,
            QtCore.Qt.Tool | QtCore.Qt.FramelessWindowHint | QtCore.Qt.WindowStaysOnTopHint,
        )
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self.setAttribute(QtCore.Qt.WA_TranslucentBackground, True)
        self.setCursor(QtCore.Qt.CrossCursor)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.setGeometry(self._screenGeometry())

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        painter.fillRect(self.rect(), QtGui.QColor(0, 0, 0, 70))
        painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255), 1))
        painter.drawText(self.rect(), QtCore.Qt.AlignCenter, _loc("COLOUR_PICKER_PICK_SCREEN_HINT"))

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() != QtCore.Qt.LeftButton:
            return
        colour = self._sampleScreen(event.globalPos())
        if colour is not None:
            self.colourPicked.emit(colour)
        self.close()

    def keyPressEvent(self, event: QtGui.QKeyEvent) -> None:
        if event.key() == QtCore.Qt.Key_Escape:
            self.close()
            return
        super().keyPressEvent(event)

    def _screenGeometry(self) -> QtCore.QRect:
        rect = QtCore.QRect()
        for screen in QtWidgets.QApplication.screens():
            rect = screen.geometry() if rect.isNull() else rect.united(screen.geometry())
        primary = QtWidgets.QApplication.primaryScreen()
        if rect.isNull() and primary is not None:
            rect = primary.geometry()
        return rect

    def _sampleScreen(self, globalPos: QtCore.QPoint) -> QtGui.QColor | None:
        screen = next(
            (candidate for candidate in QtWidgets.QApplication.screens() if candidate.geometry().contains(globalPos)),
            QtWidgets.QApplication.primaryScreen(),
        )
        if screen is None:
            return None
        geometry = screen.geometry()
        pixmap = screen.grabWindow(0, globalPos.x() - geometry.x(), globalPos.y() - geometry.y(), 1, 1)
        if pixmap.isNull():
            return None
        return pixmap.toImage().pixelColor(0, 0)


class ColourPickerDialog(QmlDialogHost):
    screenColourPicked = QtCore.pyqtSignal(
        int,
        int,
        int,
        int,
        arguments=("red", "green", "blue", "alpha"),
    )
    customColoursChanged = QtCore.pyqtSignal("QVariantList", arguments=("colours",))

    _BASIC_COLOURS = [
        "#000000", "#404040", "#808080", "#C0C0C0", "#FFFFFF", "#800000", "#FF0000", "#FF8080",
        "#808000", "#FFFF00", "#FFFF80", "#008000", "#00FF00", "#80FF80", "#008080", "#00FFFF",
        "#80FFFF", "#000080", "#0000FF", "#8080FF", "#800080", "#FF00FF", "#FF80FF", "#804000",
        "#FF8000", "#FFC080", "#804040", "#C06060", "#A0A000", "#60A060", "#408080", "#4060A0",
        "#6040A0", "#A04080", "#202020", "#606060", "#A0A0A0", "#E0E0E0", "#A00000", "#00A000",
        "#0000A0", "#A0A000", "#00A0A0", "#A000A0", "#A06000", "#0060A0", "#60A000", "#A00060",
    ]
    _customColours: list[QtGui.QColor | None] = [None] * 16

    def __init__(self, parent: QtWidgets.QWidget | None, value: Any) -> None:
        self._initialColour = QColorFromValue(value)
        self._colour = QColorFromValue(value)
        self._screenAlpha = self._colour.alpha()
        self._screenOverlay: _ScreenColourOverlay | None = None
        labels = [_loc(key) for key in (
            "COLOUR_PICKER_CURRENT", "COLOUR_PICKER_NEW", "COLOUR_PICKER_HUE",
            "COLOUR_PICKER_SATURATION", "COLOUR_PICKER_VALUE", "COLOUR_PICKER_RED",
            "COLOUR_PICKER_GREEN", "COLOUR_PICKER_BLUE", "COLOUR_PICKER_ALPHA",
            "COLOUR_PICKER_HTML",
        )]
        super().__init__(
            parent,
            _loc("COLOUR_PICKER_TITLE"),
            QtCore.QSize(820, 500),
            QtCore.QSize(700, 460),
            labels,
        )
        self.loadQml(
            "Dialogs/ColourPickerDialog.qml",
            {
                "colourPickerInitial": list(ColourTupleFromValue(self._initialColour)),
                "colourPickerBasicColours": list(self._BASIC_COLOURS),
                "colourPickerCustomColours": self._customColourData(),
            },
        )

    def getValue(self) -> tuple[int, int, int, int]:
        return ColourTupleFromValue(self._colour)

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        self._colour = QtGui.QColor(
            _clampChannel(result.get("r")),
            _clampChannel(result.get("g")),
            _clampChannel(result.get("b")),
            _clampChannel(result.get("a"), 255),
        )
        return True

    @QtCore.pyqtSlot(int)
    def pickScreenColour(self, alpha: int) -> None:
        self._screenAlpha = _clampChannel(alpha, 255)
        if self._screenOverlay is not None:
            self._screenOverlay.raise_()
            return
        overlay = _ScreenColourOverlay(self)
        overlay.colourPicked.connect(self._onScreenColourPicked)
        overlay.destroyed.connect(self._onScreenOverlayDestroyed)
        self._screenOverlay = overlay
        overlay.show()
        overlay.activateWindow()
        overlay.setFocus()

    @QtCore.pyqtSlot(int, int, int, int)
    def addCustomColour(self, r: int, g: int, b: int, a: int) -> None:
        colour = QtGui.QColor(
            _clampChannel(r), _clampChannel(g), _clampChannel(b), _clampChannel(a, 255)
        )
        colours = [QtGui.QColor(colour)] + [
            existing
            for existing in self._customColours
            if existing is not None and existing != colour
        ]
        colours = colours[:16]
        colours.extend([None] * (16 - len(colours)))
        self.__class__._customColours = colours
        self.customColoursChanged.emit(self._customColourData())

    def _customColourData(self) -> list[object]:
        return [
            None if colour is None else list(ColourTupleFromValue(colour))
            for colour in self._customColours
        ]

    def _onScreenColourPicked(self, colour: QtGui.QColor) -> None:
        colour.setAlpha(self._screenAlpha)
        self.screenColourPicked.emit(colour.red(), colour.green(), colour.blue(), colour.alpha())

    def _onScreenOverlayDestroyed(self) -> None:
        self._screenOverlay = None


class ColourVarEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(object)
    qmlValueChanged = QtCore.pyqtSignal(
        int,
        int,
        int,
        int,
        arguments=("red", "green", "blue", "alpha"),
    )

    def __init__(self, value: Any = None, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._asTuple = not isinstance(value, list)
        self._value = ColourTupleFromValue(value)
        self.setFixedSize(54, 28)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        quickWidget = QtQuickWidgets.QQuickWidget(self)
        quickWidget.setResizeMode(QtQuickWidgets.QQuickWidget.SizeRootObjectToView)
        quickWidget.setClearColor(QtCore.Qt.transparent)
        quickWidget.setFocusPolicy(QtCore.Qt.StrongFocus)
        context = quickWidget.rootContext()
        context.setContextProperty("colourEditor", self)
        context.setContextProperty("colourEditorInitial", list(self._value))
        context.setContextProperty("dialogTheme", buildQmlTheme(self))
        quickWidget.setSource(QtCore.QUrl.fromLocalFile(self._qmlPath()))
        if quickWidget.status() == QtQuickWidgets.QQuickWidget.Error:
            errors = "\n".join(error.toString() for error in quickWidget.errors())
            raise RuntimeError(f"Failed to load QML colour editor:\n{errors}")
        self._quickWidget = quickWidget
        layout.addWidget(quickWidget)

    def _qmlPath(self) -> str:
        relative = os.path.join("EditorGlobal", "Qml", "Controls", "ColourVarEditor.qml")
        candidates = (
            os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), relative),
            os.path.join(os.path.dirname(sys.executable), relative),
            os.path.join(os.getcwd(), relative),
        )
        for path in candidates:
            if os.path.isfile(path):
                return os.path.abspath(path)
        raise FileNotFoundError(os.path.abspath(candidates[0]))

    def getValue(self) -> tuple[int, int, int, int] | list[int]:
        return self._value if self._asTuple else list(self._value)

    def setValue(self, value: Any, emit: bool = True, preserveContainer: bool = False) -> None:
        if not preserveContainer:
            self._asTuple = not isinstance(value, list)
        newValue = ColourTupleFromValue(value)
        if newValue == self._value:
            return
        self._value = newValue
        self.qmlValueChanged.emit(*self._value)
        if emit:
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        self._quickWidget.setEnabled(editable)

    @QtCore.pyqtSlot()
    def openPicker(self) -> None:
        dialog = ColourPickerDialog(GetIndependentDialogParent(self), self._value)

        def onFinished(code: int) -> None:
            dialog.finished.disconnect(onFinished)
            if code != QtWidgets.QDialog.Accepted or not IsWidgetValid(self):
                return
            self.setValue(dialog.getValue(), preserveContainer=True)

        dialog.finished.connect(onFinished)
        dialog.open()
