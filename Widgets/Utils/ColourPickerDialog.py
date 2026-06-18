# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import builtins
from typing import Any

from PyQt5 import QtCore, QtGui, QtWidgets


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


def _drawCheckerboard(painter: QtGui.QPainter, rect: QtCore.QRect, size: int = 5) -> None:
    light = QtGui.QColor(210, 210, 210)
    dark = QtGui.QColor(150, 150, 150)
    for y in range(rect.top(), rect.bottom() + 1, size):
        for x in range(rect.left(), rect.right() + 1, size):
            colour = light if ((x // size) + (y // size)) % 2 == 0 else dark
            painter.fillRect(QtCore.QRect(x, y, size, size), colour)


def colourTupleFromValue(value: Any) -> tuple[int, int, int, int]:
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

    for attrs in (("r", "g", "b", "a"), ("red", "green", "blue", "alpha")):
        if all(hasattr(value, attr) for attr in attrs):
            return tuple(_clampChannel(getattr(value, attr), 255 if attr == attrs[-1] else 0) for attr in attrs)

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


def qColorFromValue(value: Any) -> QtGui.QColor:
    return QtGui.QColor(*colourTupleFromValue(value))


class _ColourPlane(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(int, int)

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._hue = 0
        self._saturation = 0
        self._value = 255
        self.setMinimumSize(240, 180)
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)

    def setState(self, hue: int, saturation: int, value: int) -> None:
        self._hue = max(0, min(359, int(hue)))
        self._saturation = max(0, min(255, int(saturation)))
        self._value = max(0, min(255, int(value)))
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        rect = self.rect().adjusted(1, 1, -2, -2)

        hueColour = QtGui.QColor.fromHsv(self._hue, 255, 255)
        satGradient = QtGui.QLinearGradient(rect.topLeft(), rect.topRight())
        satGradient.setColorAt(0.0, QtGui.QColor(255, 255, 255))
        satGradient.setColorAt(1.0, hueColour)
        painter.fillRect(rect, satGradient)

        valueGradient = QtGui.QLinearGradient(rect.topLeft(), rect.bottomLeft())
        valueGradient.setColorAt(0.0, QtGui.QColor(0, 0, 0, 0))
        valueGradient.setColorAt(1.0, QtGui.QColor(0, 0, 0, 255))
        painter.fillRect(rect, valueGradient)

        x = rect.left() + round(self._saturation * max(1, rect.width() - 1) / 255)
        y = rect.top() + round((255 - self._value) * max(1, rect.height() - 1) / 255)
        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        painter.setPen(QtGui.QPen(QtGui.QColor(0, 0, 0), 3))
        painter.drawEllipse(QtCore.QPoint(x, y), 6, 6)
        painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255), 1))
        painter.drawEllipse(QtCore.QPoint(x, y), 6, 6)

        painter.setRenderHint(QtGui.QPainter.Antialiasing, False)
        painter.setPen(QtGui.QPen(QtGui.QColor(55, 55, 55), 1))
        painter.drawRect(rect)

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() == QtCore.Qt.LeftButton:
            self._setFromPos(event.pos())

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.buttons() & QtCore.Qt.LeftButton:
            self._setFromPos(event.pos())

    def _setFromPos(self, pos: QtCore.QPoint) -> None:
        rect = self.rect().adjusted(1, 1, -2, -2)
        x = max(rect.left(), min(rect.right(), pos.x()))
        y = max(rect.top(), min(rect.bottom(), pos.y()))
        self._saturation = round((x - rect.left()) * 255 / max(1, rect.width() - 1))
        self._value = 255 - round((y - rect.top()) * 255 / max(1, rect.height() - 1))
        self.VALUE_CHANGED.emit(self._saturation, self._value)
        self.update()


class _HueSlider(QtWidgets.QWidget):
    HUE_CHANGED = QtCore.pyqtSignal(int)

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._hue = 0
        self.setFixedWidth(28)
        self.setMinimumHeight(180)

    def setHue(self, hue: int) -> None:
        self._hue = max(0, min(359, int(hue)))
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        rect = self.rect().adjusted(7, 1, -8, -2)

        gradient = QtGui.QLinearGradient(rect.topLeft(), rect.bottomLeft())
        stops = (
            (0.0, 0),
            (1 / 6, 60),
            (2 / 6, 120),
            (3 / 6, 180),
            (4 / 6, 240),
            (5 / 6, 300),
            (1.0, 359),
        )
        for pos, hue in stops:
            gradient.setColorAt(pos, QtGui.QColor.fromHsv(hue, 255, 255))
        painter.fillRect(rect, gradient)

        y = rect.top() + round(self._hue * max(1, rect.height() - 1) / 359)
        painter.setPen(QtGui.QPen(QtGui.QColor(0, 0, 0), 2))
        painter.drawLine(2, y, self.width() - 3, y)
        painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255), 1))
        painter.drawLine(3, y, self.width() - 4, y)

        painter.setPen(QtGui.QPen(QtGui.QColor(55, 55, 55), 1))
        painter.drawRect(rect)

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() == QtCore.Qt.LeftButton:
            self._setFromPos(event.pos())

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.buttons() & QtCore.Qt.LeftButton:
            self._setFromPos(event.pos())

    def _setFromPos(self, pos: QtCore.QPoint) -> None:
        rect = self.rect().adjusted(7, 1, -8, -2)
        y = max(rect.top(), min(rect.bottom(), pos.y()))
        self._hue = round((y - rect.top()) * 359 / max(1, rect.height() - 1))
        self.HUE_CHANGED.emit(self._hue)
        self.update()


class _ColourPreview(QtWidgets.QWidget):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._colour = QtGui.QColor(255, 255, 255, 255)
        self.setFixedSize(76, 32)

    def setColour(self, colour: QtGui.QColor) -> None:
        self._colour = QtGui.QColor(colour)
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        rect = self.rect().adjusted(1, 1, -2, -2)
        _drawCheckerboard(painter, rect, 5)
        painter.fillRect(rect, self._colour)
        painter.setPen(QtGui.QPen(QtGui.QColor(55, 55, 55), 1))
        painter.drawRect(rect)


class _ColourGrid(QtWidgets.QWidget):
    COLOUR_SELECTED = QtCore.pyqtSignal(object)

    def __init__(
        self,
        colours: list[QtGui.QColor | None],
        columns: int,
        parent: QtWidgets.QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._colours = colours
        self._columns = max(1, columns)
        self._cell = 20
        self._gap = 3
        rows = (len(colours) + self._columns - 1) // self._columns
        self.setFixedSize(
            self._columns * self._cell + max(0, self._columns - 1) * self._gap,
            rows * self._cell + max(0, rows - 1) * self._gap,
        )

    def setColours(self, colours: list[QtGui.QColor | None]) -> None:
        self._colours = colours
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        for i, colour in enumerate(self._colours):
            rect = self._cellRect(i)
            if colour is None:
                painter.fillRect(rect, QtGui.QColor(38, 38, 38))
                painter.setPen(QtGui.QPen(QtGui.QColor(82, 82, 82), 1))
                painter.drawLine(rect.topLeft(), rect.bottomRight())
            else:
                _drawCheckerboard(painter, rect, 4)
                painter.fillRect(rect, colour)
            painter.setPen(QtGui.QPen(QtGui.QColor(70, 70, 70), 1))
            painter.drawRect(rect.adjusted(0, 0, -1, -1))

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() != QtCore.Qt.LeftButton:
            return
        index = self._indexAt(event.pos())
        if index is None:
            return
        colour = self._colours[index]
        if colour is not None:
            self.COLOUR_SELECTED.emit(QtGui.QColor(colour))

    def _cellRect(self, index: int) -> QtCore.QRect:
        row = index // self._columns
        col = index % self._columns
        return QtCore.QRect(
            col * (self._cell + self._gap),
            row * (self._cell + self._gap),
            self._cell,
            self._cell,
        )

    def _indexAt(self, pos: QtCore.QPoint) -> int | None:
        for i in range(len(self._colours)):
            if self._cellRect(i).contains(pos):
                return i
        return None


class _ScreenColourOverlay(QtWidgets.QWidget):
    COLOUR_PICKED = QtCore.pyqtSignal(object)

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent, QtCore.Qt.Tool | QtCore.Qt.FramelessWindowHint | QtCore.Qt.WindowStaysOnTopHint)
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
            self.COLOUR_PICKED.emit(colour)
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
        if rect.isNull():
            rect = QtWidgets.QApplication.primaryScreen().geometry()
        return rect

    def _sampleScreen(self, globalPos: QtCore.QPoint) -> QtGui.QColor | None:
        screen = None
        for candidate in QtWidgets.QApplication.screens():
            if candidate.geometry().contains(globalPos):
                screen = candidate
                break
        if screen is None:
            screen = QtWidgets.QApplication.primaryScreen()
        if screen is None:
            return None
        geom = screen.geometry()
        pixmap = screen.grabWindow(0, globalPos.x() - geom.x(), globalPos.y() - geom.y(), 1, 1)
        if pixmap.isNull():
            return None
        return pixmap.toImage().pixelColor(0, 0)


class ColourPickerDialog(QtWidgets.QDialog):
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
        from Utils import System
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.SetStyle(self, "singleRow.qss")
        self._syncing = False
        self._screenOverlay: _ScreenColourOverlay | None = None
        self._initialColour = qColorFromValue(value)
        self._colour = qColorFromValue(value)
        hsv = self._colour.toHsv()
        self._hue = max(0, hsv.hue())
        self._saturation = hsv.saturation()
        self._value = hsv.value()

        self.setWindowTitle(_loc("COLOUR_PICKER_TITLE"))
        self.setMinimumWidth(660)

        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(10)

        body = QtWidgets.QHBoxLayout()
        body.setSpacing(12)
        root.addLayout(body, 1)

        selectorLayout = QtWidgets.QHBoxLayout()
        selectorLayout.setSpacing(6)
        body.addLayout(selectorLayout, 1)

        self._plane = _ColourPlane(self)
        self._hueSlider = _HueSlider(self)
        selectorLayout.addWidget(self._plane, 1)
        selectorLayout.addWidget(self._hueSlider, 0)

        palettesLayout = QtWidgets.QVBoxLayout()
        palettesLayout.setSpacing(8)
        body.addLayout(palettesLayout, 0)

        basicLabel = QtWidgets.QLabel(_loc("COLOUR_PICKER_BASIC_COLOURS"), self)
        palettesLayout.addWidget(basicLabel)
        self._basicGrid = _ColourGrid([QtGui.QColor(v) for v in self._BASIC_COLOURS], 8, self)
        palettesLayout.addWidget(self._basicGrid)

        customLabel = QtWidgets.QLabel(_loc("COLOUR_PICKER_CUSTOM_COLOURS"), self)
        palettesLayout.addWidget(customLabel)
        self._customGrid = _ColourGrid(self._customColours, 8, self)
        palettesLayout.addWidget(self._customGrid)

        self._pickScreenBtn = QtWidgets.QPushButton(_loc("COLOUR_PICKER_PICK_SCREEN"), self)
        self._addCustomBtn = QtWidgets.QPushButton(_loc("COLOUR_PICKER_ADD_CUSTOM"), self)
        palettesLayout.addWidget(self._pickScreenBtn)
        palettesLayout.addWidget(self._addCustomBtn)
        palettesLayout.addStretch(1)

        formWidget = QtWidgets.QWidget(self)
        form = QtWidgets.QFormLayout(formWidget)
        form.setContentsMargins(0, 0, 0, 0)
        form.setSpacing(8)
        body.addWidget(formWidget, 0)

        self._oldPreview = _ColourPreview(self)
        self._newPreview = _ColourPreview(self)
        form.addRow(_loc("COLOUR_PICKER_CURRENT"), self._oldPreview)
        form.addRow(_loc("COLOUR_PICKER_NEW"), self._newPreview)

        self._hSpin = self._createRangeSpin(0, 359)
        self._sSpin = self._createChannelSpin()
        self._vSpin = self._createChannelSpin()
        form.addRow(_loc("COLOUR_PICKER_HUE"), self._hSpin)
        form.addRow(_loc("COLOUR_PICKER_SATURATION"), self._sSpin)
        form.addRow(_loc("COLOUR_PICKER_VALUE"), self._vSpin)

        self._rSpin = self._createChannelSpin()
        self._gSpin = self._createChannelSpin()
        self._bSpin = self._createChannelSpin()
        self._aSpin = self._createChannelSpin()
        form.addRow(_loc("COLOUR_PICKER_RED"), self._rSpin)
        form.addRow(_loc("COLOUR_PICKER_GREEN"), self._gSpin)
        form.addRow(_loc("COLOUR_PICKER_BLUE"), self._bSpin)
        form.addRow(_loc("COLOUR_PICKER_ALPHA"), self._aSpin)

        self._hexEdit = QtWidgets.QLineEdit(self)
        self._hexEdit.setFixedWidth(96)
        self._hexEdit.setMaxLength(9)
        form.addRow(_loc("COLOUR_PICKER_HTML"), self._hexEdit)

        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        okBtn = buttons.button(QtWidgets.QDialogButtonBox.Ok)
        cancelBtn = buttons.button(QtWidgets.QDialogButtonBox.Cancel)
        if okBtn:
            okBtn.setText(_loc("CONFIRM"))
        if cancelBtn:
            cancelBtn.setText(_loc("CANCEL"))
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        root.addWidget(buttons)

        self._plane.VALUE_CHANGED.connect(self._onPlaneChanged)
        self._hueSlider.HUE_CHANGED.connect(self._onHueChanged)
        self._basicGrid.COLOUR_SELECTED.connect(lambda colour: self._setColour(colour, preserveAlpha=True))
        self._customGrid.COLOUR_SELECTED.connect(lambda colour: self._setColour(colour))
        self._pickScreenBtn.clicked.connect(self._pickScreenColour)
        self._addCustomBtn.clicked.connect(self._addCustomColour)
        for spin in (self._hSpin, self._sSpin, self._vSpin):
            spin.valueChanged.connect(self._onHsvChanged)
        for spin in (self._rSpin, self._gSpin, self._bSpin, self._aSpin):
            spin.valueChanged.connect(self._onChannelChanged)
        self._hexEdit.editingFinished.connect(self._onHexChanged)

        self._syncWidgets()

    def getValue(self) -> tuple[int, int, int, int]:
        return colourTupleFromValue(self._colour)

    def _createRangeSpin(self, minimum: int, maximum: int) -> QtWidgets.QSpinBox:
        spin = QtWidgets.QSpinBox(self)
        spin.setRange(minimum, maximum)
        spin.setFixedWidth(76)
        return spin

    def _createChannelSpin(self) -> QtWidgets.QSpinBox:
        return self._createRangeSpin(0, 255)

    def _syncWidgets(self) -> None:
        self._syncing = True
        self._plane.setState(self._hue, self._saturation, self._value)
        self._hueSlider.setHue(self._hue)
        self._oldPreview.setColour(self._initialColour)
        self._newPreview.setColour(self._colour)
        self._hSpin.setValue(self._hue)
        self._sSpin.setValue(self._saturation)
        self._vSpin.setValue(self._value)
        self._rSpin.setValue(self._colour.red())
        self._gSpin.setValue(self._colour.green())
        self._bSpin.setValue(self._colour.blue())
        self._aSpin.setValue(self._colour.alpha())
        self._hexEdit.setText(_formatHexColour(self._colour))
        self._syncing = False

    def _setColour(self, colour: QtGui.QColor, preserveAlpha: bool = False) -> None:
        alpha = self._colour.alpha() if preserveAlpha else colour.alpha()
        self._colour = QtGui.QColor(colour.red(), colour.green(), colour.blue(), alpha)
        self._setHsvFromColour()
        self._syncWidgets()

    def _setColourFromHsv(self) -> None:
        self._colour = QtGui.QColor.fromHsv(self._hue, self._saturation, self._value, self._colour.alpha())

    def _setHsvFromColour(self) -> None:
        hsv = self._colour.toHsv()
        hue = hsv.hue()
        if hue >= 0:
            self._hue = hue
        self._saturation = hsv.saturation()
        self._value = hsv.value()

    def _onPlaneChanged(self, saturation: int, value: int) -> None:
        if self._syncing:
            return
        self._saturation = saturation
        self._value = value
        self._setColourFromHsv()
        self._syncWidgets()

    def _onHueChanged(self, hue: int) -> None:
        if self._syncing:
            return
        self._hue = hue
        self._setColourFromHsv()
        self._syncWidgets()

    def _onHsvChanged(self) -> None:
        if self._syncing:
            return
        self._hue = self._hSpin.value()
        self._saturation = self._sSpin.value()
        self._value = self._vSpin.value()
        self._setColourFromHsv()
        self._syncWidgets()

    def _onChannelChanged(self) -> None:
        if self._syncing:
            return
        self._colour = QtGui.QColor(
            self._rSpin.value(),
            self._gSpin.value(),
            self._bSpin.value(),
            self._aSpin.value(),
        )
        self._setHsvFromColour()
        self._syncWidgets()

    def _onHexChanged(self) -> None:
        if self._syncing:
            return
        parsed = _parseHexColourText(self._hexEdit.text(), self._colour.alpha())
        if parsed is None:
            self._syncWidgets()
            return
        self._colour = QtGui.QColor(*parsed)
        self._setHsvFromColour()
        self._syncWidgets()

    def _addCustomColour(self) -> None:
        colours = list(self._customColours)
        colours = [QtGui.QColor(self._colour)] + [c for c in colours if c is not None and c != self._colour]
        colours = colours[:16]
        while len(colours) < 16:
            colours.append(None)
        self.__class__._customColours = colours
        self._customGrid.setColours(self._customColours)

    def _pickScreenColour(self) -> None:
        if self._screenOverlay is not None:
            self._screenOverlay.raise_()
            return
        overlay = _ScreenColourOverlay(self)
        overlay.COLOUR_PICKED.connect(self._onScreenColourPicked)
        overlay.destroyed.connect(lambda: setattr(self, "_screenOverlay", None))
        self._screenOverlay = overlay
        overlay.show()
        overlay.activateWindow()
        overlay.setFocus()

    def _onScreenColourPicked(self, colour: QtGui.QColor) -> None:
        colour.setAlpha(self._colour.alpha())
        self._setColour(colour)


class ColourSwatchButton(QtWidgets.QPushButton):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._colour = QtGui.QColor(255, 255, 255, 255)
        self.setFixedSize(54, 28)
        self.setCursor(QtCore.Qt.PointingHandCursor)
        self.setText("")

    def setColour(self, colour: QtGui.QColor) -> None:
        self._colour = QtGui.QColor(colour)
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        option = QtWidgets.QStyleOptionButton()
        option.initFrom(self)
        option.state |= QtWidgets.QStyle.State_Raised
        if self.isDown():
            option.state |= QtWidgets.QStyle.State_Sunken
        self.style().drawControl(QtWidgets.QStyle.CE_PushButtonBevel, option, painter, self)

        rect = self.rect().adjusted(5, 5, -5, -5)
        self._drawCheckerboard(painter, rect)
        painter.fillRect(rect, self._colour)
        painter.setPen(QtGui.QPen(QtGui.QColor(35, 35, 35), 1))
        painter.drawRect(rect.adjusted(0, 0, -1, -1))

    def _drawCheckerboard(self, painter: QtGui.QPainter, rect: QtCore.QRect) -> None:
        _drawCheckerboard(painter, rect, 5)


class ColourVarEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, value: Any = None, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._asTuple = not isinstance(value, list)
        self._value = colourTupleFromValue(value)

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        self._swatch = ColourSwatchButton(self)
        self._swatch.clicked.connect(self._openDialog)
        layout.addWidget(self._swatch, 0)
        layout.addStretch()
        self._syncSwatch()

    def getValue(self) -> tuple[int, int, int, int] | list[int]:
        return self._value if self._asTuple else list(self._value)

    def setValue(self, value: Any, emit: bool = True, preserveContainer: bool = False) -> None:
        if not preserveContainer:
            self._asTuple = not isinstance(value, list)
        newValue = colourTupleFromValue(value)
        if newValue == self._value:
            return
        self._value = newValue
        self._syncSwatch()
        if emit:
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        self._swatch.setEnabled(editable)

    def _syncSwatch(self) -> None:
        self._swatch.setColour(QtGui.QColor(*self._value))

    def _openDialog(self) -> None:
        dlg = ColourPickerDialog(QtWidgets.QApplication.activeWindow(), self._value)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        self.setValue(dlg.getValue(), preserveContainer=True)
