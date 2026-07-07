# -*- encoding: utf-8 -*-

from __future__ import annotations

import json
import os
import re
import runpy
import sys
import traceback
from contextlib import contextmanager
from dataclasses import dataclass
from types import SimpleNamespace
from typing import Any, Callable, Dict, List, Optional

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import EditorStatus
from . import Locale


_PLUGIN_TYPES = {"submenu", "rightclick", "panel", "tab", "toolbar", "datatype", "textInputHover", "textInputHint"}
_MENU_NAMES = {"FILE", "EDIT", "GAME", "DATABASE", "HELP"}


@contextmanager
def _pluginSysPath(path: str):
    absPath = os.path.abspath(path)
    oldPath = list(sys.path)
    if absPath not in {os.path.abspath(item) for item in sys.path if isinstance(item, str)}:
        sys.path.insert(0, absPath)
    try:
        yield
    finally:
        sys.path[:] = oldPath


@dataclass
class _PluginRecord:
    name: str
    rootDir: str
    config: Dict[str, Any]
    editor: SimpleNamespace
    namespace: Dict[str, Any]


class PluginManager:
    _loaded = False
    _records: List[_PluginRecord] = []
    _rightClickHooks: List[tuple[_PluginRecord, Dict[str, Any]]] = []
    _datatypeHooks: List[tuple[_PluginRecord, Dict[str, Any], Dict[str, Any]]] = []
    _textInputHoverHooks: List[tuple[_PluginRecord, Callable[..., Any]]] = []
    _textInputHintHooks: List[tuple[_PluginRecord, Callable[..., Any]]] = []
    _mainWindow: Optional[Any] = None

    @classmethod
    def Init(cls, mainWindow: Optional[Any] = None) -> None:
        if cls._loaded:
            cls.SetMainWindow(mainWindow)
            return
        cls._loaded = True
        cls._records = []
        cls._rightClickHooks = []
        cls._datatypeHooks = []
        cls._textInputHoverHooks = []
        cls._textInputHintHooks = []
        cls._mainWindow = mainWindow
        from . import File

        pluginsRoot = os.path.join(File.GetRootPath(), "Plugins")
        if not os.path.isdir(pluginsRoot):
            return
        for entry in sorted(os.listdir(pluginsRoot)):
            rootDir = os.path.join(pluginsRoot, entry)
            if not os.path.isdir(rootDir):
                continue
            record = cls._loadPlugin(entry, rootDir, mainWindow)
            if record is not None:
                cls._records.append(record)
        cls._collectRightClickHooks()
        cls._registerDatatypeHooks()
        cls._registerTextInputHoverHooks()
        cls._registerTextInputHintHooks()

    @classmethod
    def SetMainWindow(cls, mainWindow: Optional[Any]) -> None:
        if mainWindow is None:
            return
        cls._mainWindow = mainWindow
        for record in cls._records:
            record.editor.main_window = mainWindow
            record.editor.project_path = EditorStatus.PROJ_PATH

    @classmethod
    def InstallWindowPlugins(cls, window: Any, menuMap: Dict[str, QtWidgets.QMenu]) -> None:
        cls.Init(window)
        if bool(getattr(window, "_pluginSystemInstalled", False)):
            return
        setattr(window, "_pluginSystemInstalled", True)
        cls._installSubmenus(window, menuMap)
        cls._installToolbars(window)
        cls._installPanels(window)
        cls._installTabs(window)
        cls._installConsoleFilter(window)
        cls._installTextInputHover()
        cls._installTextInputHint()

    @classmethod
    def ResolveTextInputHintSuffix(
        cls,
        widget: QtWidgets.QWidget,
        text: str,
        cursorIndex: int,
    ) -> Optional[str]:
        cls.Init(cls._mainWindow)
        window = cls._mainWindow
        for record, func in cls._textInputHintHooks:
            try:
                result = func(window, widget, text, cursorIndex)
            except Exception:
                cls._warn(
                    cls._mainWindow,
                    f"Text input hint hook failed in plugin {record.name}:\n{traceback.format_exc()}",
                )
                continue
            if isinstance(result, str) and result:
                return result
        return None

    @classmethod
    def ResolveTextInputHoverTooltip(
        cls,
        widget: QtWidgets.QWidget,
        text: str,
        cursorIndex: int,
    ) -> Optional[str]:
        cls.Init(cls._mainWindow)
        parts: List[str] = []
        window = cls._mainWindow
        for record, func in cls._textInputHoverHooks:
            try:
                result = func(window, widget, text, cursorIndex)
            except Exception:
                cls._warn(
                    cls._mainWindow,
                    f"Text input hover hook failed in plugin {record.name}:\n{traceback.format_exc()}",
                )
                continue
            if isinstance(result, str) and result.strip():
                parts.append(result.strip())
        if not parts:
            return None
        return "\n\n".join(parts)

    @classmethod
    def AddRightClickActions(
        cls,
        menu: QtWidgets.QMenu,
        owner: QtWidgets.QWidget,
        location: str,
        condition: str = "always",
        itemData: Any = None,
    ) -> int:
        cls.Init(cls._mainWindow)
        window = cls._mainWindow
        if window is None:
            from . import File

            if hasattr(File, "mainWindow"):
                window = File.mainWindow
        if window is None:
            return 0
        hooks = cls._matchingRightClickHooks(location, condition)
        added = 0
        for record, hook in hooks:
            action = cls._createRightClickAction(record, hook, window, owner, location, condition, itemData)
            if action is None:
                continue
            cls._insertMenuAction(menu, action, hook)
            added += 1
        return added

    @classmethod
    def ApplyPanelForEditMode(cls, window: Any, editMode: int) -> bool:
        panels = getattr(window, "_pluginPanels", [])
        for entry in panels:
            modes = entry.get("editModes")
            widget = entry.get("widget")
            if isinstance(widget, QtWidgets.QWidget) and editMode in modes:
                window.rightStack.setCurrentWidget(widget)
                return True
        return False

    @classmethod
    def HandleDataFile(cls, parent: QtWidgets.QWidget, path: str, data: Dict[str, Any]) -> bool:
        cls.Init(cls._mainWindow)
        typeName = data.get("type")
        for _record, config, handlers in cls._datatypeHooks:
            hookTypeName = config.get("typeName")
            if typeName != hookTypeName and not cls._isPluginDataPath(path, str(hookTypeName)):
                continue
            editorFactory = handlers.get("editor")
            if not callable(editorFactory):
                return False
            try:
                widget = editorFactory()
            except Exception:
                cls._warn(parent, f"Failed to open plugin data editor for {hookTypeName}:\n{traceback.format_exc()}")
                return True
            if isinstance(widget, QtWidgets.QWidget):
                widget.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
                widget.show()
                widget.raise_()
                widget.activateWindow()
            return True
        return False

    @classmethod
    def LoadPluginDataFile(cls, path: str) -> Optional[Dict[str, Any]]:
        cls.Init(cls._mainWindow)
        for _record, config, handlers in cls._datatypeHooks:
            if not cls._isPluginDataPath(path, str(config.get("typeName", ""))):
                continue
            extension = str(config.get("extension", "")).lower()
            if os.path.splitext(path)[1].lower() != extension:
                continue
            loader = handlers.get("loader")
            try:
                if callable(loader):
                    data = loader(path)
                elif config.get("defaultFormat", "json") == "dat":
                    from . import File

                    data = File.LoadData(path)
                else:
                    from . import File

                    data = File.GetJSONData(path)
            except Exception:
                return None
            return data if isinstance(data, dict) else None
        return None

    @classmethod
    def PluginDataHooks(cls) -> List[tuple[str, Dict[str, Any], Dict[str, Any]]]:
        cls.Init(cls._mainWindow)
        return [(record.name, config, handlers) for record, config, handlers in cls._datatypeHooks]

    @classmethod
    def _loadPlugin(cls, folderName: str, rootDir: str, mainWindow: Optional[Any]) -> Optional[_PluginRecord]:
        configPath = os.path.join(rootDir, "Plugin.json")
        pluginPath = os.path.join(rootDir, "Plugin.py")
        if not os.path.isfile(configPath) or not os.path.isfile(pluginPath):
            return None
        try:
            with open(configPath, "r", encoding="utf-8") as file:
                config = json.load(file)
        except Exception:
            cls._warn(mainWindow, f"Failed to read plugin config: {configPath}\n{traceback.format_exc()}")
            return None
        error = cls._validateConfig(folderName, config)
        if error:
            cls._warn(mainWindow, error)
            return None
        Locale.MergeLocaleJson(os.path.join(rootDir, "locale.json"))
        editor = SimpleNamespace(
            main_window=mainWindow,
            project_path=EditorStatus.PROJ_PATH,
            plugin_dir=os.path.abspath(rootDir),
            plugin_config=config,
        )
        try:
            with _pluginSysPath(rootDir):
                namespace = runpy.run_path(pluginPath, init_globals={"EDITOR": editor}, run_name="__main__")
        except Exception:
            cls._warn(mainWindow, f"Failed to load plugin {folderName}:\n{traceback.format_exc()}")
            return None
        return _PluginRecord(str(config["name"]), os.path.abspath(rootDir), config, editor, namespace)

    @classmethod
    def _validateConfig(cls, folderName: str, config: Any) -> str:
        if not isinstance(config, dict):
            return f"Plugin {folderName} has invalid Plugin.json root."
        name = config.get("name")
        if not isinstance(name, str) or not name:
            return f"Plugin {folderName} has no valid name."
        if name != folderName:
            return f"Plugin {folderName} skipped: name must match folder name."
        version = config.get("version")
        if not isinstance(version, str) or not version:
            return f"Plugin {folderName} has no valid version."
        types = config.get("types")
        if not isinstance(types, list) or not all(isinstance(item, str) for item in types):
            return f"Plugin {folderName} has no valid types list."
        invalidTypes = [item for item in types if item not in _PLUGIN_TYPES]
        if invalidTypes:
            return f"Plugin {folderName} has invalid types: {', '.join(invalidTypes)}."
        hooks = config.get("hooks")
        if not isinstance(hooks, dict):
            return f"Plugin {folderName} has no valid hooks object."
        minVersion = config.get("minEditorVersion")
        if isinstance(minVersion, str) and minVersion and cls._compareVersion(EditorStatus.VERSION, minVersion) < 0:
            return f"Plugin {folderName} requires Ludork {minVersion} or newer."
        return ""

    @classmethod
    def _compareVersion(cls, left: str, right: str) -> int:
        leftParts = cls._versionParts(left)
        rightParts = cls._versionParts(right)
        count = max(len(leftParts), len(rightParts))
        for i in range(count):
            lv = leftParts[i] if i < len(leftParts) else 0
            rv = rightParts[i] if i < len(rightParts) else 0
            if lv != rv:
                return 1 if lv > rv else -1
        return 0

    @staticmethod
    def _versionParts(value: str) -> List[int]:
        return [int(part) if part.isdigit() else 0 for part in re.split(r"[.\-+_]", value)]

    @classmethod
    def _collectRightClickHooks(cls) -> None:
        cls._rightClickHooks = []
        for record in cls._records:
            hooks = record.config.get("hooks", {})
            entries = hooks.get("rightclick", [])
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if isinstance(entry, dict):
                    cls._rightClickHooks.append((record, entry))

    @classmethod
    def _registerDatatypeHooks(cls) -> None:
        cls._datatypeHooks = []
        for record in cls._records:
            hooks = record.config.get("hooks", {})
            entries = hooks.get("datatype", [])
            if isinstance(entries, dict):
                entries = [entries]
            if not isinstance(entries, list):
                continue
            handlerFactory = record.namespace.get("hook_datatype")
            handlers: Dict[str, Any] = {}
            if callable(handlerFactory):
                try:
                    result = handlerFactory()
                    if isinstance(result, dict):
                        handlers = result
                except Exception:
                    cls._warn(cls._mainWindow, f"Datatype hook failed in plugin {record.name}:\n{traceback.format_exc()}")
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                typeName = entry.get("typeName")
                extension = entry.get("extension")
                if not isinstance(typeName, str) or not typeName or not isinstance(extension, str) or not extension:
                    continue
                if not extension.startswith("."):
                    entry["extension"] = "." + extension
                cls._datatypeHooks.append((record, entry, handlers))
        if cls._datatypeHooks:
            from EditorGlobal import GameData

            GameData.SetPluginDataTypes(cls.PluginDataHooks())

    @classmethod
    def _registerTextInputHoverHooks(cls) -> None:
        cls._textInputHoverHooks = []
        for record in cls._records:
            if "textInputHover" not in record.config.get("types", []):
                continue
            func = record.namespace.get("hook_text_input_hover")
            if callable(func):
                cls._textInputHoverHooks.append((record, func))

    @classmethod
    def _registerTextInputHintHooks(cls) -> None:
        cls._textInputHintHooks = []
        for record in cls._records:
            if "textInputHint" not in record.config.get("types", []):
                continue
            func = record.namespace.get("hook_text_input_hint")
            if callable(func):
                cls._textInputHintHooks.append((record, func))

    @classmethod
    def _installTextInputHover(cls) -> None:
        from . import TextInputHover

        TextInputHover.InstallApplicationFilter()

    @classmethod
    def _installTextInputHint(cls) -> None:
        from . import TextInputHint

        TextInputHint.InstallApplicationFilter()

    @classmethod
    def _installSubmenus(cls, window: Any, menuMap: Dict[str, QtWidgets.QMenu]) -> None:
        for record in cls._records:
            entries = record.config.get("hooks", {}).get("submenu", [])
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                parentName = str(entry.get("parentMenu", "")).upper()
                if parentName not in _MENU_NAMES:
                    continue
                parentMenu = menuMap.get(parentName)
                if parentMenu is None:
                    continue
                action = cls._createConfiguredAction(record, entry, window, "submenu")
                if action is None:
                    continue
                cls._insertMenuAction(parentMenu, action, entry)

    @classmethod
    def _installToolbars(cls, window: Any) -> None:
        topLayout = window.topBar.layout()
        if not isinstance(topLayout, QtWidgets.QHBoxLayout):
            return
        for record in cls._records:
            entries = record.config.get("hooks", {}).get("toolbar", [])
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                widget = cls._createToolbarWidget(record, entry, window)
                if widget is None:
                    continue
                index = cls._toolbarInsertIndex(topLayout, window)
                topLayout.insertWidget(index, widget, 0, alignment=QtCore.Qt.AlignRight)

    @classmethod
    def _installPanels(cls, window: Any) -> None:
        installed = []
        for record in cls._records:
            entries = record.config.get("hooks", {}).get("panel", [])
            if isinstance(entries, dict):
                entries = [entries]
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                func = record.namespace.get("hook_panel")
                if not callable(func):
                    continue
                try:
                    widget = func(window)
                except Exception:
                    cls._warn(window, f"Panel hook failed in plugin {record.name}:\n{traceback.format_exc()}")
                    continue
                if not isinstance(widget, QtWidgets.QWidget):
                    continue
                window.rightStack.addWidget(widget)
                modes = entry.get("editModes", [0, 1, 2])
                if not isinstance(modes, list):
                    modes = [0, 1, 2]
                installed.append({"widget": widget, "editModes": [int(v) for v in modes if isinstance(v, int)]})
        setattr(window, "_pluginPanels", installed)
        cls.ApplyPanelForEditMode(window, int(getattr(window, "_editModeIdx", 0)))

    @classmethod
    def _installTabs(cls, window: Any) -> None:
        for record in cls._records:
            entries = record.config.get("hooks", {}).get("tab", [])
            if isinstance(entries, dict):
                entries = [entries]
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                func = record.namespace.get("hook_tab")
                if not callable(func):
                    continue
                try:
                    widget = func(window)
                except Exception:
                    cls._warn(window, f"Tab hook failed in plugin {record.name}:\n{traceback.format_exc()}")
                    continue
                if not isinstance(widget, QtWidgets.QWidget):
                    continue
                label = cls._text(entry)
                icon = cls._icon(record, entry.get("icon"))
                if icon is not None:
                    window.tabWidget.addTab(widget, icon, label)
                else:
                    window.tabWidget.addTab(widget, label)

    @classmethod
    def _installConsoleFilter(cls, window: Any) -> None:
        consoleWidget = getattr(window, "consoleWidget", None)
        if not isinstance(consoleWidget, QtWidgets.QWidget):
            return
        if bool(getattr(consoleWidget, "_pluginConsoleFilterInstalled", False)):
            return
        filterButton = getattr(consoleWidget, "_filterButton", None)
        if not isinstance(filterButton, QtWidgets.QToolButton):
            return
        menu = filterButton.menu()
        if not isinstance(menu, QtWidgets.QMenu):
            return
        cls.AddRightClickActions(menu, consoleWidget, "consoleFilter", "always", None)
        setattr(consoleWidget, "_pluginConsoleFilterInstalled", True)

    @classmethod
    def _createConfiguredAction(
        cls, record: _PluginRecord, config: Dict[str, Any], window: Any, hookType: str
    ) -> Optional[QtWidgets.QAction]:
        label = str(config.get("label") or config.get("tabLabel") or "")
        funcName = f"hook_{hookType}_{cls._snake(label)}"
        func = record.namespace.get(funcName)
        if callable(func):
            try:
                action = func(window)
            except Exception:
                cls._warn(window, f"{hookType} hook failed in plugin {record.name}:\n{traceback.format_exc()}")
                return None
            if action is None:
                return None
            if isinstance(action, QtWidgets.QAction):
                cls._applyActionConfig(record, action, config)
                return action
            return None
        action = QtWidgets.QAction(cls._text(config), window)
        cls._applyActionConfig(record, action, config)
        cls._connectDefaultTrigger(record, action, window, hookType, config, None, None)
        return action

    @classmethod
    def _createRightClickAction(
        cls,
        record: _PluginRecord,
        config: Dict[str, Any],
        window: Any,
        owner: QtWidgets.QWidget,
        baseLocation: str,
        condition: str,
        itemData: Any,
    ) -> Optional[QtWidgets.QAction]:
        location = str(config.get("location", ""))
        candidates = [f"hook_rightclick_{location}", f"hook_rightclick_{baseLocation}"]
        for funcName in candidates:
            func = record.namespace.get(funcName)
            if callable(func):
                try:
                    action = func(window, condition, itemData)
                except Exception:
                    cls._warn(owner, f"Right click hook failed in plugin {record.name}:\n{traceback.format_exc()}")
                    return None
                if action is None:
                    return None
                if isinstance(action, QtWidgets.QAction):
                    cls._applyActionConfig(record, action, config)
                    return action
                return None
        action = QtWidgets.QAction(cls._text(config), owner)
        cls._applyActionConfig(record, action, config)
        cls._connectDefaultTrigger(record, action, window, "rightclick", config, condition, itemData)
        return action

    @classmethod
    def _createToolbarWidget(
        cls, record: _PluginRecord, config: Dict[str, Any], window: Any
    ) -> Optional[QtWidgets.QWidget]:
        label = str(config.get("label") or "")
        func = record.namespace.get(f"hook_toolbar_{cls._snake(label)}")
        if callable(func):
            try:
                widget = func(window)
            except Exception:
                cls._warn(window, f"Toolbar hook failed in plugin {record.name}:\n{traceback.format_exc()}")
                return None
            return widget if isinstance(widget, QtWidgets.QWidget) else None
        button = QtWidgets.QToolButton(window)
        button.setText(cls._text(config))
        button.setToolTip(cls._text(config))
        button.setCheckable(config.get("mode") == "toggle")
        icon = cls._icon(record, config.get("icon"))
        if icon is not None:
            button.setIcon(icon)
        defaultTrigger = record.namespace.get("on_triggered")
        if callable(defaultTrigger):
            button.clicked.connect(lambda checked=False, c=config, f=defaultTrigger: f(window, "toolbar", c))
        return button

    @classmethod
    def _applyActionConfig(cls, record: _PluginRecord, action: QtWidgets.QAction, config: Dict[str, Any]) -> None:
        if not action.text():
            action.setText(cls._text(config))
        if "checkable" in config:
            action.setCheckable(bool(config.get("checkable")))
        shortcut = config.get("shortcut")
        if isinstance(shortcut, str) and shortcut:
            action.setShortcut(QtGui.QKeySequence(shortcut))
        icon = cls._icon(record, config.get("icon"))
        if icon is not None and action.icon().isNull():
            action.setIcon(icon)

    @classmethod
    def _connectDefaultTrigger(
        cls,
        record: _PluginRecord,
        action: QtWidgets.QAction,
        window: Any,
        hookType: str,
        config: Dict[str, Any],
        condition: Optional[str],
        itemData: Any,
    ) -> None:
        trigger = record.namespace.get("on_triggered")
        if not callable(trigger):
            return

        def _run(checked: bool = False) -> None:
            if condition is None:
                trigger(window, hookType, config)
            else:
                trigger(window, hookType, config, condition, itemData)

        action.triggered.connect(_run)

    @classmethod
    def _matchingRightClickHooks(cls, location: str, condition: str) -> List[tuple[_PluginRecord, Dict[str, Any]]]:
        keys = {location, f"{location}_{condition}"}
        result = []
        for record, hook in cls._rightClickHooks:
            hookLocation = hook.get("location")
            hookCondition = hook.get("condition", "always")
            if hookLocation not in keys:
                continue
            if hookCondition not in ("always", condition):
                continue
            result.append((record, hook))
        return result

    @classmethod
    def _insertMenuAction(cls, menu: QtWidgets.QMenu, action: QtWidgets.QAction, config: Dict[str, Any]) -> None:
        if bool(config.get("separatorBefore")):
            menu.addSeparator()
        position = str(config.get("position", "last"))
        anchor = config.get("anchor")
        anchorAction = cls._findAnchorAction(menu, anchor) if isinstance(anchor, str) and anchor else None
        if position == "first":
            actions = menu.actions()
            if actions:
                menu.insertAction(actions[0], action)
            else:
                menu.addAction(action)
        elif position == "before" and anchorAction is not None:
            menu.insertAction(anchorAction, action)
        elif position == "after" and anchorAction is not None:
            actions = menu.actions()
            idx = actions.index(anchorAction)
            before = actions[idx + 1] if idx + 1 < len(actions) else None
            menu.insertAction(before, action)
        else:
            menu.addAction(action)
        if bool(config.get("separatorAfter")):
            menu.addSeparator()

    @classmethod
    def _findAnchorAction(cls, menu: QtWidgets.QMenu, anchorKey: str) -> Optional[QtWidgets.QAction]:
        anchorText = cls._normaliseMenuText(Locale.GetContent(anchorKey))
        for action in menu.actions():
            if cls._normaliseMenuText(action.text()) == anchorText:
                return action
        return None

    @staticmethod
    def _normaliseMenuText(text: str) -> str:
        return str(text).replace("&", "").strip()

    @staticmethod
    def _snake(value: str) -> str:
        text = re.sub(r"[^0-9A-Za-z]+", "_", value).strip("_")
        text = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", text)
        return text.lower()

    @staticmethod
    def _text(config: Dict[str, Any]) -> str:
        key = config.get("localeKey")
        if isinstance(key, str) and key:
            return Locale.GetContent(key)
        return str(config.get("label") or config.get("tabLabel") or "")

    @classmethod
    def _icon(cls, record: _PluginRecord, value: Any) -> Optional[QtGui.QIcon]:
        if not isinstance(value, str) or not value:
            return None
        candidates = []
        if os.path.isabs(value):
            candidates.append(value)
        else:
            from . import File

            candidates.append(os.path.join(record.rootDir, value))
            candidates.append(os.path.join(File.GetRootPath(), value))
        for path in candidates:
            if os.path.exists(path):
                return QtGui.QIcon(path)
        return None

    @staticmethod
    def _toolbarInsertIndex(layout: QtWidgets.QHBoxLayout, window: Any) -> int:
        for i in range(layout.count()):
            item = layout.itemAt(i)
            if item is not None and item.widget() is window.editModeToggle:
                return i
        return layout.count()

    @staticmethod
    def _isPluginDataPath(path: str, typeName: str) -> bool:
        if not path or not typeName:
            return False
        dataRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Data", typeName))
        try:
            return os.path.commonpath([os.path.abspath(path), dataRoot]) == dataRoot
        except Exception:
            return False

    @staticmethod
    def _warn(parent: Optional[Any], message: str) -> None:
        print(message)
        app = QtWidgets.QApplication.instance()
        if app is None:
            return
        widget = parent if isinstance(parent, QtWidgets.QWidget) else None
        QtWidgets.QMessageBox.warning(widget, "Plugin", message)


def Init(mainWindow: Optional[Any] = None) -> None:
    PluginManager.Init(mainWindow)


def InstallWindowPlugins(window: Any, menuMap: Dict[str, QtWidgets.QMenu]) -> None:
    PluginManager.InstallWindowPlugins(window, menuMap)


def AddRightClickActions(
    menu: QtWidgets.QMenu,
    owner: QtWidgets.QWidget,
    location: str,
    condition: str = "always",
    itemData: Any = None,
) -> int:
    return PluginManager.AddRightClickActions(menu, owner, location, condition, itemData)


def ApplyPanelForEditMode(window: Any, editMode: int) -> bool:
    return PluginManager.ApplyPanelForEditMode(window, editMode)


def HandleDataFile(parent: QtWidgets.QWidget, path: str, data: Dict[str, Any]) -> bool:
    return PluginManager.HandleDataFile(parent, path, data)


def LoadPluginDataFile(path: str) -> Optional[Dict[str, Any]]:
    return PluginManager.LoadPluginDataFile(path)


def ResolveTextInputHoverTooltip(
    widget: QtWidgets.QWidget,
    text: str,
    cursorIndex: int,
) -> Optional[str]:
    return PluginManager.ResolveTextInputHoverTooltip(widget, text, cursorIndex)


def ResolveTextInputHintSuffix(
    widget: QtWidgets.QWidget,
    text: str,
    cursorIndex: int,
) -> Optional[str]:
    return PluginManager.ResolveTextInputHintSuffix(widget, text, cursorIndex)
