# -*- encoding: utf-8 -*-

from __future__ import annotations

import weakref
from typing import Set, Union

from PyQt5 import QtCore, QtGui, QtWidgets

_TEXT_INPUT_TYPES = (
    QtWidgets.QLineEdit,
    QtWidgets.QPlainTextEdit,
    QtWidgets.QTextEdit,
)

_HINT_GAP = 8
_HINT_TEXT_COLOR = "#888888"
_CONTROLLER_ATTR = "_ludorkTextInputHintController"
_controllers: Set[weakref.ReferenceType[_TextInputHintController]] = set()


def _registerController(controller: _TextInputHintController) -> None:
    _controllers.add(weakref.ref(controller))


def refreshAll() -> None:
    for ref in list(_controllers):
        controller = ref()
        if controller is None:
            _controllers.discard(ref)
            continue
        controller.refresh()


def _applyHintStyle(label: QtWidgets.QLabel) -> None:
    label.setObjectName("LudorkTextInputHint")
    color = QtGui.QColor(_HINT_TEXT_COLOR)
    palette = label.palette()
    palette.setColor(QtGui.QPalette.WindowText, color)
    palette.setColor(QtGui.QPalette.Text, color)
    label.setPalette(palette)
    label.setStyleSheet(
        f"QLabel#LudorkTextInputHint {{ color: {_HINT_TEXT_COLOR}; background: transparent; border: none; padding: 0px; margin: 0px; }}"
    )


def _lineEditContentsRect(lineEdit: QtWidgets.QLineEdit) -> QtCore.QRect:
    option = QtWidgets.QStyleOptionFrame()
    lineEdit.initStyleOption(option)
    return lineEdit.style().subElementRect(QtWidgets.QStyle.SE_LineEditContents, option, lineEdit)


def _lineEditTextEndRect(lineEdit: QtWidgets.QLineEdit) -> QtCore.QRect:
    contents = _lineEditContentsRect(lineEdit)
    if contents.width() <= 0 or contents.height() <= 0:
        return QtCore.QRect()
    text = lineEdit.text()
    fontMetrics = lineEdit.fontMetrics()
    lineHeight = max(1, fontMetrics.height())
    y = contents.top() + max(0, (contents.height() - lineHeight) // 2)
    textWidth = fontMetrics.horizontalAdvance(text) if text else 0
    xEnd = contents.left() + textWidth
    if text and textWidth > contents.width():
        savedPosition = lineEdit.cursorPosition()
        lineEdit.blockSignals(True)
        try:
            lineEdit.setCursorPosition(len(text))
            scrolledEnd = lineEdit.cursorRect().right()
            if scrolledEnd > contents.left():
                xEnd = scrolledEnd
        finally:
            lineEdit.setCursorPosition(savedPosition)
            lineEdit.blockSignals(False)
    return QtCore.QRect(max(xEnd, contents.left()), y, 1, lineHeight)


def _plainTextEndRect(plainEdit: Union[QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit]) -> QtCore.QRect:
    viewport = plainEdit.viewport()
    if viewport.width() <= 0 or viewport.height() <= 0:
        return QtCore.QRect()
    text = plainEdit.toPlainText()
    fontMetrics = plainEdit.fontMetrics()
    lineHeight = max(1, fontMetrics.height())
    if not text:
        return QtCore.QRect(0, 0, 0, lineHeight)
    cursor = plainEdit.textCursor()
    oldPosition = cursor.position()
    cursor.setPosition(len(text))
    endRect = plainEdit.cursorRect(cursor)
    cursor.setPosition(oldPosition)
    plainEdit.setTextCursor(cursor)
    if endRect.height() > 0:
        return endRect
    textWidth = fontMetrics.horizontalAdvance(text)
    y = max(0, (viewport.height() - lineHeight) // 2)
    return QtCore.QRect(textWidth, y, 1, lineHeight)


def _placeHintLabel(
    label: QtWidgets.QLabel,
    endRect: QtCore.QRect,
    rightBound: int,
) -> None:
    if endRect.width() == 0 and endRect.height() == 0:
        label.hide()
        return
    x = endRect.right() + _HINT_GAP
    y = endRect.top()
    height = max(1, endRect.height())
    availableWidth = max(0, rightBound - x + 1)
    if availableWidth <= 0:
        label.hide()
        return
    label.setGeometry(x, y, availableWidth, height)
    label.show()
    label.raise_()


def _scheduleRefresh(controller: _TextInputHintController) -> None:
    QtCore.QTimer.singleShot(0, controller.refresh)


def _attachIfNeeded(widget: QtWidgets.QWidget) -> None:
    if getattr(widget, _CONTROLLER_ATTR, None) is not None:
        return
    if isinstance(widget, QtWidgets.QLineEdit):
        if widget.echoMode() != QtWidgets.QLineEdit.Normal:
            return
        controller = _LineEditHintController(widget)
    elif isinstance(widget, (QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
        controller = _PlainTextHintController(widget)
    else:
        return
    setattr(widget, _CONTROLLER_ATTR, controller)
    _registerController(controller)
    _scheduleRefresh(controller)


class _TextInputHintController(QtCore.QObject):
    def refresh(self) -> None:
        raise NotImplementedError


class _LineEditHintController(_TextInputHintController):
    def __init__(self, lineEdit: QtWidgets.QLineEdit) -> None:
        super().__init__(lineEdit)
        self._lineEdit = lineEdit
        self._hintLabel = QtWidgets.QLabel(lineEdit)
        self._hintLabel.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
        self._hintLabel.setAttribute(QtCore.Qt.WA_TranslucentBackground, True)
        self._hintLabel.setFocusPolicy(QtCore.Qt.NoFocus)
        self._hintLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self._hintLabel.hide()
        lineEdit.installEventFilter(self)
        lineEdit.textChanged.connect(self.refresh)
        lineEdit.cursorPositionChanged.connect(self.refresh)
        _applyHintStyle(self._hintLabel)

    def eventFilter(self, receiver: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if receiver is self._lineEdit and event.type() in (
            QtCore.QEvent.Resize,
            QtCore.QEvent.Show,
            QtCore.QEvent.Paint,
            QtCore.QEvent.FocusIn,
            QtCore.QEvent.PaletteChange,
        ):
            self.refresh()
        return super().eventFilter(receiver, event)

    def refresh(self) -> None:
        from . import PluginSystem

        if self._lineEdit.width() <= 0:
            _scheduleRefresh(self)
            return
        _applyHintStyle(self._hintLabel)
        text = self._lineEdit.text()
        cursorIndex = self._lineEdit.cursorPosition()
        hint = PluginSystem.ResolveTextInputHintSuffix(self._lineEdit, text, cursorIndex)
        if not hint:
            self._hintLabel.clear()
            self._hintLabel.hide()
            return
        endRect = _lineEditTextEndRect(self._lineEdit)
        if endRect.isNull():
            _scheduleRefresh(self)
            return
        self._hintLabel.setFont(self._lineEdit.font())
        self._hintLabel.setText(hint)
        _placeHintLabel(
            self._hintLabel,
            endRect,
            _lineEditContentsRect(self._lineEdit).right(),
        )


class _PlainTextHintController(_TextInputHintController):
    def __init__(self, plainEdit: Union[QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit]) -> None:
        super().__init__(plainEdit)
        self._plainEdit = plainEdit
        self._hintLabel = QtWidgets.QLabel(plainEdit.viewport())
        self._hintLabel.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
        self._hintLabel.setAttribute(QtCore.Qt.WA_TranslucentBackground, True)
        self._hintLabel.setFocusPolicy(QtCore.Qt.NoFocus)
        self._hintLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self._hintLabel.hide()
        plainEdit.installEventFilter(self)
        plainEdit.textChanged.connect(self.refresh)
        plainEdit.cursorPositionChanged.connect(self.refresh)
        _applyHintStyle(self._hintLabel)

    def eventFilter(self, receiver: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if receiver is self._plainEdit and event.type() in (
            QtCore.QEvent.Resize,
            QtCore.QEvent.Show,
            QtCore.QEvent.Paint,
            QtCore.QEvent.FocusIn,
            QtCore.QEvent.PaletteChange,
        ):
            self.refresh()
        return super().eventFilter(receiver, event)

    def refresh(self) -> None:
        from . import PluginSystem

        viewport = self._plainEdit.viewport()
        if viewport.width() <= 0:
            _scheduleRefresh(self)
            return
        _applyHintStyle(self._hintLabel)
        text = self._plainEdit.toPlainText()
        cursorIndex = self._plainEdit.textCursor().position()
        hint = PluginSystem.ResolveTextInputHintSuffix(self._plainEdit, text, cursorIndex)
        if not hint:
            self._hintLabel.clear()
            self._hintLabel.hide()
            return
        endRect = _plainTextEndRect(self._plainEdit)
        if endRect.isNull():
            _scheduleRefresh(self)
            return
        self._hintLabel.setFont(self._plainEdit.font())
        self._hintLabel.setText(hint)
        _placeHintLabel(self._hintLabel, endRect, viewport.width() - 2)


class _TextInputHintAttachFilter(QtCore.QObject):
    def eventFilter(self, receiver: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if event.type() == QtCore.QEvent.Show and isinstance(receiver, _TEXT_INPUT_TYPES):
            _attachIfNeeded(receiver)
        elif event.type() == QtCore.QEvent.ChildAdded:
            child = event.child()
            if isinstance(child, _TEXT_INPUT_TYPES):
                QtCore.QTimer.singleShot(0, lambda w=child: _attachIfNeeded(w))
        return super().eventFilter(receiver, event)


_filterInstalled = False


def InstallApplicationFilter() -> None:
    global _filterInstalled
    if _filterInstalled:
        return
    app = QtWidgets.QApplication.instance()
    if app is None:
        return
    app.installEventFilter(_TextInputHintAttachFilter(app))
    for widget in app.allWidgets():
        if isinstance(widget, _TEXT_INPUT_TYPES):
            _attachIfNeeded(widget)
    _filterInstalled = True
