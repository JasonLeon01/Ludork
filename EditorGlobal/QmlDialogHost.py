# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import sys
import weakref
from collections.abc import Mapping, Sequence
from typing import Optional

from PyQt5 import QtCore, QtGui, QtQml, QtQuickWidgets, QtWidgets

_activeDialogHosts: set[weakref.ReferenceType["QmlDialogHost"]] = set()


def _withAlpha(value: str, alpha: float) -> str:
    colour = QtGui.QColor(value)
    colour.setAlphaF(alpha)
    return colour.name(QtGui.QColor.HexArgb)


def _themeIconUrl(state: str, fileName: str) -> str:
    if state not in ("active", "disabled", "primary") or os.path.basename(fileName) != fileName:
        return ""
    for root in QtCore.QDir.searchPaths("icon"):
        path = os.path.join(root, state, fileName)
        if os.path.isfile(path):
            return QtCore.QUrl.fromLocalFile(os.path.abspath(path)).toString()
    return ""


def buildQmlTheme(widget: QtWidgets.QWidget) -> dict[str, str | int]:
    app = QtWidgets.QApplication.instance()
    if isinstance(app, QtWidgets.QApplication):
        font = app.font()
        palette = app.palette()
    else:
        font = widget.font()
        palette = widget.palette()
    fontPixelSize = font.pixelSize() if font.pixelSize() > 0 else 12
    tokenNames = (
        "PRIMARYCOLOR",
        "PRIMARYLIGHTCOLOR",
        "SECONDARYCOLOR",
        "SECONDARYLIGHTCOLOR",
        "SECONDARYDARKCOLOR",
        "PRIMARYTEXTCOLOR",
        "SECONDARYTEXTCOLOR",
    )
    tokens = {name: os.environ.get(f"QTMATERIAL_{name}", "") for name in tokenNames}
    if all(QtGui.QColor(value).isValid() for value in tokens.values()):
        disabledText = QtGui.QColor(tokens["SECONDARYTEXTCOLOR"])
        disabledText.setAlphaF(0.3)
        colours = {
            "backgroundColor": tokens["SECONDARYDARKCOLOR"],
            "surfaceColor": tokens["SECONDARYCOLOR"],
            "alternateSurfaceColor": tokens["SECONDARYLIGHTCOLOR"],
            "textColor": tokens["SECONDARYTEXTCOLOR"],
            "disabledTextColor": disabledText.name(QtGui.QColor.HexArgb),
            "borderColor": tokens["SECONDARYLIGHTCOLOR"],
            "inputBorderColor": _withAlpha(tokens["SECONDARYTEXTCOLOR"], 0.2),
            "disabledSurfaceColor": _withAlpha(tokens["SECONDARYCOLOR"], 0.3),
            "accentColor": tokens["PRIMARYCOLOR"],
            "accentTextColor": tokens["PRIMARYTEXTCOLOR"],
            "selectionColor": tokens["PRIMARYLIGHTCOLOR"],
            "focusOverlayColor": _withAlpha(tokens["PRIMARYCOLOR"], 0.2),
        }
    else:
        colours = {
            "backgroundColor": palette.window().color().name(),
            "surfaceColor": palette.base().color().name(),
            "alternateSurfaceColor": palette.alternateBase().color().name(),
            "textColor": palette.windowText().color().name(),
            "disabledTextColor": palette.color(QtGui.QPalette.Disabled, QtGui.QPalette.WindowText).name(),
            "borderColor": palette.mid().color().name(),
            "inputBorderColor": _withAlpha(palette.windowText().color().name(), 0.2),
            "disabledSurfaceColor": _withAlpha(palette.base().color().name(), 0.3),
            "accentColor": palette.highlight().color().name(),
            "accentTextColor": palette.highlightedText().color().name(),
            "selectionColor": palette.highlight().color().name(),
            "focusOverlayColor": _withAlpha(palette.highlight().color().name(), 0.2),
        }
    return {
        "fontFamily": font.family(),
        "fontPixelSize": fontPixelSize,
        "formLabelWidth": 0,
        **colours,
        "activeUpArrowIcon": _themeIconUrl("active", "uparrow.svg"),
        "disabledUpArrowIcon": _themeIconUrl("disabled", "uparrow.svg"),
        "activeDownArrowIcon": _themeIconUrl("active", "downarrow.svg"),
        "primaryDownArrowIcon": _themeIconUrl("primary", "downarrow.svg"),
        "disabledDownArrowIcon": _themeIconUrl("disabled", "downarrow.svg"),
        "primaryCheckedBoxIcon": _themeIconUrl("primary", "checkbox_checked.svg"),
        "primaryUncheckedBoxIcon": _themeIconUrl("primary", "checkbox_unchecked.svg"),
        "primaryIndeterminateBoxIcon": _themeIconUrl("primary", "checkbox_indeterminate.svg"),
        "disabledCheckedBoxIcon": _themeIconUrl("disabled", "checkbox_checked.svg"),
        "disabledUncheckedBoxIcon": _themeIconUrl("disabled", "checkbox_unchecked.svg"),
        "disabledIndeterminateBoxIcon": _themeIconUrl("disabled", "checkbox_indeterminate.svg"),
        "primaryCheckedRadioIcon": _themeIconUrl("primary", "radiobutton_checked.svg"),
        "primaryUncheckedRadioIcon": _themeIconUrl("primary", "radiobutton_unchecked.svg"),
        "disabledCheckedRadioIcon": _themeIconUrl("disabled", "radiobutton_checked.svg"),
        "disabledUncheckedRadioIcon": _themeIconUrl("disabled", "radiobutton_unchecked.svg"),
    }


class QmlDialogHost(QtWidgets.QDialog):
    textInputHintRefreshRequested = QtCore.pyqtSignal()

    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        title: str = "",
        size: Optional[QtCore.QSize] = None,
        minimumSize: Optional[QtCore.QSize] = None,
        formLabels: Optional[Sequence[str]] = None,
    ) -> None:
        super().__init__(parent)
        if not QtWidgets.QApplication.testAttribute(QtCore.Qt.AA_ShareOpenGLContexts):
            raise RuntimeError("QmlDialogHost requires Qt.AA_ShareOpenGLContexts before QApplication is created")

        self._loaded = False
        self._fontFamily = ""
        self._fontPixelSize = 12
        self._colours: dict[str, str] = {}
        self._qmlTheme = buildQmlTheme(self)
        self._fontFamily = str(self._qmlTheme["fontFamily"])
        self._fontPixelSize = int(self._qmlTheme["fontPixelSize"])
        self._colours = {
            key.removesuffix("Color"): str(value)
            for key, value in self._qmlTheme.items()
            if key.endswith("Color")
        }
        metrics = QtGui.QFontMetrics(self.font())
        self._formLabelWidth = max((metrics.horizontalAdvance(label) for label in formLabels or ()), default=0)
        self._qmlTheme["formLabelWidth"] = self._formLabelWidth

        self.setWindowTitle(title)
        self.setWindowModality(QtCore.Qt.WindowModal if parent is not None else QtCore.Qt.ApplicationModal)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        if minimumSize is not None:
            self.setMinimumSize(minimumSize)
        if size is not None:
            self.resize(size)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._quickWidget = QtQuickWidgets.QQuickWidget(self)
        self._quickWidget.setResizeMode(QtQuickWidgets.QQuickWidget.SizeRootObjectToView)
        self._quickWidget.setClearColor(QtGui.QColor(self._colours["background"]))
        self._quickWidget.setFocusPolicy(QtCore.Qt.StrongFocus)
        layout.addWidget(self._quickWidget)
        _activeDialogHosts.add(weakref.ref(self))

    @classmethod
    def refreshTextInputHints(cls) -> None:
        for ref in list(_activeDialogHosts):
            host = ref()
            if host is None:
                _activeDialogHosts.discard(ref)
                continue
            host.textInputHintRefreshRequested.emit()

    @QtCore.pyqtProperty(str, constant=True)
    def fontFamily(self) -> str:
        return self._fontFamily

    @QtCore.pyqtProperty(int, constant=True)
    def fontPixelSize(self) -> int:
        return self._fontPixelSize

    @QtCore.pyqtProperty(int, constant=True)
    def formLabelWidth(self) -> int:
        return self._formLabelWidth

    @QtCore.pyqtProperty(str, constant=True)
    def backgroundColor(self) -> str:
        return self._colours["background"]

    @QtCore.pyqtProperty(str, constant=True)
    def surfaceColor(self) -> str:
        return self._colours["surface"]

    @QtCore.pyqtProperty(str, constant=True)
    def alternateSurfaceColor(self) -> str:
        return self._colours["alternateSurface"]

    @QtCore.pyqtProperty(str, constant=True)
    def textColor(self) -> str:
        return self._colours["text"]

    @QtCore.pyqtProperty(str, constant=True)
    def disabledTextColor(self) -> str:
        return self._colours["disabledText"]

    @QtCore.pyqtProperty(str, constant=True)
    def borderColor(self) -> str:
        return self._colours["border"]

    @QtCore.pyqtProperty(str, constant=True)
    def inputBorderColor(self) -> str:
        return self._colours["inputBorder"]

    @QtCore.pyqtProperty(str, constant=True)
    def disabledSurfaceColor(self) -> str:
        return self._colours["disabledSurface"]

    @QtCore.pyqtProperty(str, constant=True)
    def accentColor(self) -> str:
        return self._colours["accent"]

    @QtCore.pyqtProperty(str, constant=True)
    def accentTextColor(self) -> str:
        return self._colours["accentText"]

    @QtCore.pyqtProperty(str, constant=True)
    def selectionColor(self) -> str:
        return self._colours["selection"]

    @QtCore.pyqtProperty(str, constant=True)
    def focusOverlayColor(self) -> str:
        return self._colours["focusOverlay"]

    @QtCore.pyqtSlot(str, result=str)
    def localize(self, key: str) -> str:
        return ELOC(key)

    @QtCore.pyqtSlot(str, int, result=str)
    def textInputHintSuffix(self, text: str, cursorIndex: int) -> str:
        from Utils import PluginSystem

        return PluginSystem.ResolveTextInputHintSuffix(self, text, cursorIndex) or ""

    @QtCore.pyqtSlot(str, str, result=str)
    def themeIconUrl(self, state: str, fileName: str) -> str:
        return _themeIconUrl(state, fileName)

    def _dialogTheme(self) -> dict[str, str | int]:
        return dict(self._qmlTheme)

    def loadQml(self, fileName: str, contextProperties: Optional[Mapping[str, object]] = None) -> None:
        if self._loaded:
            raise RuntimeError("QmlDialogHost source has already been loaded")
        context = self._quickWidget.rootContext()
        if context is None:
            raise RuntimeError("QmlDialogHost could not create a QML context")
        context.setContextProperty("dialogHost", self)
        context.setContextProperty("dialogTheme", self._dialogTheme())
        for name, value in (contextProperties or {}).items():
            context.setContextProperty(name, value)

        qmlPath = self._resolveQmlPath(fileName)
        self._quickWidget.setSource(QtCore.QUrl.fromLocalFile(qmlPath))
        if self._quickWidget.status() == QtQuickWidgets.QQuickWidget.Error:
            errors = "\n".join(error.toString() for error in self._quickWidget.errors())
            raise RuntimeError(f"Failed to load QML dialog {fileName}:\n{errors}")
        if self._quickWidget.rootObject() is None:
            raise RuntimeError(f"QML dialog {fileName} did not create a root object")
        self._loaded = True

    def _resolveQmlPath(self, fileName: str) -> str:
        relativePath = fileName.replace("/", os.sep).replace("\\", os.sep)
        candidates = (
            os.path.join(os.path.dirname(__file__), "Qml", relativePath),
            os.path.join(os.path.dirname(sys.executable), "EditorGlobal", "Qml", relativePath),
            os.path.join(os.getcwd(), "EditorGlobal", "Qml", relativePath),
        )
        for path in candidates:
            if os.path.isfile(path):
                return os.path.abspath(path)
        raise FileNotFoundError(os.path.abspath(candidates[0]))

    def _normaliseResult(self, result: object) -> object:
        if isinstance(result, QtQml.QJSValue):
            return result.toVariant()
        return result

    def _applyResult(self, result: object) -> bool:
        return True

    def _resultErrorText(self) -> str:
        return ELOC("UNEXPECTED_ERROR")

    def _canReject(self) -> bool:
        return True

    @QtCore.pyqtSlot("QVariant")
    def confirm(self, result: object) -> None:
        try:
            if self._applyResult(self._normaliseResult(result)):
                super().accept()
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), self._resultErrorText() + "\n" + str(e))

    @QtCore.pyqtSlot()
    def cancel(self) -> None:
        if self._canReject():
            super().reject()

    def open(self) -> None:
        if not self._loaded:
            raise RuntimeError("QmlDialogHost.open() called before loadQml()")
        super().open()
        self._quickWidget.setFocus(QtCore.Qt.OtherFocusReason)
        self.raise_()
        self.activateWindow()

    def _unregisterDialogHost(self) -> None:
        for ref in list(_activeDialogHosts):
            host = ref()
            if host is None or host is self:
                _activeDialogHosts.discard(ref)

    def _teardownQuickWidget(self) -> None:
        if not self._loaded:
            return
        self._unregisterDialogHost()
        self._loaded = False
        quickWidget = self._quickWidget
        quickWidget.hide()
        quickWidget.setParent(None)
        quickWidget.deleteLater()

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if not self._canReject():
            event.ignore()
            return
        super().closeEvent(event)
        QtCore.QTimer.singleShot(0, self._teardownQuickWidget)
