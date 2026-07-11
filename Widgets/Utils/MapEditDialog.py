# -*- encoding: utf-8 -*-

import os
from typing import Any, Callable, Optional

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import EditorStatus, GameData
from EditorGlobal.QmlDialogHost import QmlDialogHost
from Utils.DataConfig import (
    DATA_FILE_EXTENSIONS,
    DATA_FORMAT_DAT,
    DATA_FORMAT_EXTENSIONS,
    DATA_FORMAT_JSON,
    DATA_FORMAT_LABELS,
)
from Widgets.Utils.FilterEditDialog import EditFilterData, FilterData


class MapEditDialog(QmlDialogHost):
    bgmPathSelected = QtCore.pyqtSignal(str, arguments=("path",))
    bgsPathSelected = QtCore.pyqtSignal(str, arguments=("path",))
    fogPathSelected = QtCore.pyqtSignal(str, arguments=("path",))
    ambientColourPicked = QtCore.pyqtSignal(
        int,
        int,
        int,
        int,
        arguments=("red", "green", "blue", "alpha"),
    )

    def __init__(
        self,
        parent: QtWidgets.QWidget,
        data: dict[str, Any],
        current_key: str = "",
        title: Optional[str] = None,
        allow_current_key: bool = True,
        data_format: Optional[str] = None,
    ) -> None:
        if title is None:
            title = ELOC("MAPLIST_EDIT")

        existing_format = DATA_FORMAT_JSON if data.get("isJson") else DATA_FORMAT_DAT
        self._data_format = self._NormaliseDataFormat(data_format or existing_format)
        show_data_format = data_format is not None

        old_name = str(data.get("mapName", ""))
        old_w = int(data.get("width", 0))
        old_h = int(data.get("height", 0))
        current_light = data.get("ambientLight", [255, 255, 255, 255])
        if not isinstance(current_light, (list, tuple)) or len(current_light) < 4:
            current_light = [255, 255, 255, 255]

        labels = [
            ELOC("FILE_NAME"),
            ELOC("EDIT_MAP"),
            ELOC("MAP_WIDTH"),
            ELOC("MAP_HEIGHT"),
            ELOC("AMBIENT_LIGHT"),
            ELOC("MAP_BGM"),
            ELOC("MAP_BGS"),
            ELOC("MAP_FOG"),
        ]
        if show_data_format:
            labels.append(ELOC("DATA_FORMAT"))
        for fog_label in ("MAP_FOG_POWER", "MAP_FOG_OX", "MAP_FOG_OY", "MAP_FOG_DISTORT"):
            labels.append(ELOC(fog_label))

        super().__init__(parent, title, QtCore.QSize(640, 560), QtCore.QSize(540, 300), labels)

        self._data = data
        self._current_key = current_key
        self._allow_current_key = allow_current_key
        self._bgmFilterData: FilterData = data.get("bgmFilter") if "bgmFilter" in data else {}  # type: ignore[assignment]
        self._bgsFilterData: FilterData = data.get("bgsFilter") if "bgsFilter" in data else {}  # type: ignore[assignment]
        self._resultFileName = ""
        self._resultDataFormat = self._data_format
        self._bgmFilterDialog: Optional[QtWidgets.QWidget] = None
        self._bgsFilterDialog: Optional[QtWidgets.QWidget] = None

        data_format_items = [
            {"label": label, "value": fmt}
            for fmt, label in DATA_FORMAT_LABELS.items()
        ]

        proj_path = EditorStatus.PROJ_PATH
        self._bgmRoot = os.path.join(proj_path, "Assets", "Musics")
        self._bgsRoot = os.path.join(proj_path, "Assets", "Musics")
        self._fogRoot = os.path.join(proj_path, "Assets", "Fogs")

        initial_data = {
            "fileName": current_key,
            "mapName": old_name,
            "width": max(1, old_w),
            "height": max(1, old_h),
            "ambientLight": [
                int(current_light[0]),
                int(current_light[1]),
                int(current_light[2]),
                int(current_light[3]),
            ],
            "ambientR": int(current_light[0]),
            "ambientG": int(current_light[1]),
            "ambientB": int(current_light[2]),
            "ambientA": int(current_light[3]),
            "bgm": str(data.get("bgm", "")),
            "bgs": str(data.get("bgs", "")),
            "fog": str(data.get("fog", "")),
            "fogPower": int(data.get("fogPower", 0)),
            "fogOx": float(data.get("fogOx", 0)),
            "fogOy": float(data.get("fogOy", 0)),
            "fogDistort": int(data.get("fogDistort", 0)),
        }

        self.loadQml(
            "Dialogs/MapEditDialog.qml",
            {
                "mapEditInitialData": initial_data,
                "mapEditShowDataFormat": show_data_format,
                "mapEditDataFormats": data_format_items,
            },
        )

    @QtCore.pyqtSlot()
    def browseBgm(self) -> None:
        from Widgets.Utils.FileSelectorDialog import FileSelectorDialog
        dlg = FileSelectorDialog(self, self._bgmRoot, FileSelectorDialog.audioFilesFilter())
        dlg.openSelect(lambda path: self.bgmPathSelected.emit(os.path.basename(path) if path else ""))

    @QtCore.pyqtSlot()
    def browseBgs(self) -> None:
        from Widgets.Utils.FileSelectorDialog import FileSelectorDialog
        dlg = FileSelectorDialog(self, self._bgsRoot, FileSelectorDialog.audioFilesFilter())
        dlg.openSelect(lambda path: self.bgsPathSelected.emit(os.path.basename(path) if path else ""))

    @QtCore.pyqtSlot()
    def browseFog(self) -> None:
        from Widgets.Utils.FileSelectorDialog import FileSelectorDialog
        dlg = FileSelectorDialog(self, self._fogRoot, FileSelectorDialog.imageFilesFilter())
        dlg.openSelect(lambda path: self.fogPathSelected.emit(os.path.basename(path) if path else ""))

    @QtCore.pyqtSlot(int, int, int, int)
    def pickAmbientColour(self, r: int, g: int, b: int, a: int) -> None:
        from Widgets.Utils.ColourPickerDialog import ColourPickerDialog
        from Widgets.Utils.DialogUtils import GetIndependentDialogParent
        dlg = ColourPickerDialog(GetIndependentDialogParent(self), (r, g, b, a))

        def _onFinished(code: int) -> None:
            dlg.finished.disconnect(_onFinished)
            if code == QtWidgets.QDialog.Accepted:
                value = dlg.getValue()
                self.ambientColourPicked.emit(int(value[0]), int(value[1]), int(value[2]), int(value[3]))

        dlg.finished.connect(_onFinished)
        dlg.open()

    @QtCore.pyqtSlot()
    def editBgmFilter(self) -> None:
        def _onAccepted(result: FilterData) -> None:
            self._bgmFilterData = result

        self._bgmFilterDialog = EditFilterData(
            self, self._bgmFilterData, "bgm", onAccepted=_onAccepted
        )

    @QtCore.pyqtSlot()
    def editBgsFilter(self) -> None:
        def _onAccepted(result: FilterData) -> None:
            self._bgsFilterData = result

        self._bgsFilterDialog = EditFilterData(
            self, self._bgsFilterData, "bgs", onAccepted=_onAccepted
        )

    def _resultErrorText(self) -> str:
        return ELOC("GAME_CONFIG_SAVE_FAILED")

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False

        fname = str(result.get("fileName", "")).strip()
        if not fname:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("MAP_FILE_NAME_EMPTY"))
            return False

        data_format_raw = result.get("dataFormat")
        data_format = self._NormaliseDataFormat(str(data_format_raw) if data_format_raw else self._data_format)
        ext = DATA_FORMAT_EXTENSIONS[data_format]
        key = self._NormaliseFileKey(fname)
        fname = key + ext

        existing = GameData.mapData
        if key in existing:
            current_key = self._NormaliseFileKey(self._current_key)
            is_same = self._allow_current_key and current_key and key == current_key
            if not is_same:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("MAP_FILE_NAME_EXISTS"))
                return False

        self._resultFileName = fname
        self._resultDataFormat = data_format

        ambient = result.get("ambientLight", [255, 255, 255, 255])
        if not isinstance(ambient, (list, tuple)) or len(ambient) < 4:
            ambient = [255, 255, 255, 255]

        data = self._data
        old_key = self._NormaliseFileKey(self._current_key)

        GameData.RecordSnapshot()
        if old_key and old_key in GameData.mapData and key != old_key:
            new_map: dict[str, Any] = {}
            for k, v in GameData.mapData.items():
                new_map[key if k == old_key else k] = v
            GameData.mapData.clear()
            GameData.mapData.update(new_map)

        new_name = str(result.get("mapName", "")).strip()
        if new_name:
            data["mapName"] = new_name

        data["ambientLight"] = list(ambient[:4])

        bgm = str(result.get("bgm", "")).strip()
        data["bgm"] = bgm
        data["bgmFilter"] = self._bgmFilterData

        bgs = str(result.get("bgs", "")).strip()
        data["bgs"] = bgs
        data["bgsFilter"] = self._bgsFilterData

        fog = str(result.get("fog", "")).strip()
        data["fog"] = fog
        if fog:
            data["fogPower"] = int(result.get("fogPower", 0))
            data["fogOx"] = float(result.get("fogOx", 0))
            data["fogOy"] = float(result.get("fogOy", 0))
            data["fogDistort"] = int(result.get("fogDistort", 0))
        else:
            data["fogPower"] = 0
            data["fogOx"] = 0
            data["fogOy"] = 0
            data["fogDistort"] = 0

        old_w = int(data.get("width", 0))
        old_h = int(data.get("height", 0))
        new_w = int(result.get("width", old_w))
        new_h = int(result.get("height", old_h))

        if new_w != old_w or new_h != old_h:
            layers = data.get("layers", {})
            for _, layer in layers.items():
                tiles = layer.get("tiles")
                if not isinstance(tiles, list):
                    continue
                resized = []
                min_h = min(len(tiles), new_h)
                for y in range(min_h):
                    row = list(tiles[y])
                    if new_w < len(row):
                        row = row[:new_w]
                    elif new_w > len(row):
                        row.extend([None] * (new_w - len(row)))
                    resized.append(row)
                for _ in range(new_h - len(resized)):
                    resized.append([None] * new_w)
                layer["tiles"] = resized
            data["width"] = new_w
            data["height"] = new_h

        if data_format == DATA_FORMAT_JSON:
            data["isJson"] = True
        else:
            data.pop("isJson", None)

        return True

    def getFileName(self) -> str:
        return self._NormaliseFileKey(self._resultFileName)

    def getDataFormat(self) -> str:
        return self._resultDataFormat

    @staticmethod
    def _NormaliseFileKey(name: str) -> str:
        key = name.strip()
        if os.path.splitext(key)[1].lower() in DATA_FILE_EXTENSIONS:
            return os.path.splitext(key)[0]
        return key

    @staticmethod
    def _NormaliseDataFormat(data_format: Optional[str]) -> str:
        if data_format in DATA_FORMAT_EXTENSIONS:
            return data_format
        return DATA_FORMAT_JSON


def OpenMapEditDialog(
    parent: QtWidgets.QWidget,
    data: dict[str, Any],
    current_key: str = "",
    title: Optional[str] = None,
    allow_current_key: bool = True,
    data_format: Optional[str] = None,
    onAccepted: Optional[Callable[[MapEditDialog], None]] = None,
) -> MapEditDialog:
    dlg = MapEditDialog(parent, data, current_key, title, allow_current_key, data_format)
    if onAccepted is not None:
        dlg.accepted.connect(lambda: onAccepted(dlg))
    dlg.open()
    return dlg
