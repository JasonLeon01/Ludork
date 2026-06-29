# -*- encoding: utf-8 -*-

from __future__ import annotations

import json
import logging
import os
import pickle
import threading
import urllib.request
from json import JSONDecoder
from typing import Any, Optional, cast
from PyQt5 import QtWidgets, QtCore, QtGui
from EditorGlobal import EditorStatus, GameData
from Utils import File, System
from ..MarkdownRender import MarkdownToHtml
from .AiConfigDialog import AiConfigDialog, GetAiProvider, GetAiModel, GetAiApiKey, GetAiBaseUrl
from .BlueprintValidation import ValidateBlueprint

log = logging.getLogger("Ludork.Agent")

_node_index_cache: dict[str, tuple[str, str]] = {}


def _append_ludork_ai_log(message: str) -> None:
    if System.AlreadyPacked():
        return
    root = File.GetRootPath()
    if not root:
        return
    log_path = os.path.join(root, "LudorkAI.log")
    try:
        import datetime

        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S,") + f"{datetime.datetime.now().microsecond // 1000:03d}"
        with open(log_path, "a", encoding="utf-8") as handle:
            handle.write(f"{timestamp} [Ludork.Agent] {message}\n")
            handle.flush()
    except OSError:
        pass


def _ensure_log_handler() -> None:
    if not log.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(logging.Formatter("[%(name)s] %(message)s"))
        log.addHandler(handler)
        log.setLevel(logging.DEBUG)


def _agent_log(message: str, *args: object) -> None:
    if args:
        message = message % args
    _ensure_log_handler()
    log.info(message)
    _append_ludork_ai_log(message)


def _get_cached_agent_context(project_path: str, parent_class: str) -> str:
    from agent.BlueprintContext import (
        BuildParentMethodIndex,
        BuildProjectTagHints,
        GetBlueprintContextCacheKey,
    )
    from agent.NodeIndex import BuildNodeIndex

    cache_key = GetBlueprintContextCacheKey(project_path, parent_class)
    cached = _node_index_cache.get(cache_key)
    if cached is not None:
        return cached[1]

    sections: list[str] = []
    tag_hints = BuildProjectTagHints(project_path)
    if tag_hints:
        sections.append(tag_hints)
    parent_index = BuildParentMethodIndex(project_path, parent_class)
    if parent_index:
        sections.append(parent_index)
    sections.append(BuildNodeIndex(project_path))
    context_text = "\n\n".join(sections)
    _node_index_cache[cache_key] = (cache_key, context_text)
    return context_text


def _classify_intent(user_input: str, provider: str, model: str, apiKey: str, baseUrl: str) -> str:
    from agent.BlueprintContext import ClassifyIntent

    return ClassifyIntent(user_input, provider, model, apiKey, baseUrl)


def _answer_general_query(user_input: str, provider: str, model: str, apiKey: str, baseUrl: str) -> str:
    from agent.BlueprintContext import _LIGHTWEIGHT_MODELS

    lightweight = _LIGHTWEIGHT_MODELS.get(provider, model)
    postData = json.dumps({
        "model": lightweight,
        "temperature": 0.3,
        "messages": [
            {
                "role": "system",
                "content": (
                    "You are a helpful AI assistant for Ludork, a 2D RPG game "
                    "toolchain (PyQt5 editor + SFML/C++ runtime + Python). "
                    "Answer concisely and accurately."
                ),
            },
            {"role": "user", "content": user_input},
        ],
    })
    try:
        import urllib.request

        data = postData.encode("utf-8")
        url = baseUrl.rstrip("/") + "/chat/completions"
        req = urllib.request.Request(url, data=data)
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Bearer {apiKey}")
        with urllib.request.urlopen(req, timeout=60) as resp:
            result: Any = json.loads(resp.read().decode("utf-8"))
        content = result.get("choices", [{}])[0].get("message", {}).get("content", "")
        if isinstance(content, str) and content.strip():
            return content.strip()
        return ""
    except Exception as e:
        _agent_log("General query answer failed: %s", e)
        return ""


def _build_full_system_prompt(blueprint_name: str, parent_class: str, project_path: str) -> str:
    from agent.PromptResources import BuildSystemPrompt

    prompt = BuildSystemPrompt(blueprint_name, parent_class)
    if project_path and os.path.isdir(project_path):
        prompt += "\n\n" + _get_cached_agent_context(project_path, parent_class)
    return prompt


class _MarkdownBubbleContent(QtWidgets.QTextBrowser):
    def __init__(self, markdown: str, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setOpenExternalLinks(True)
        self.setFrameShape(QtWidgets.QFrame.NoFrame)
        self.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.setStyleSheet("background: transparent; border: none;")
        self.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse | QtCore.Qt.LinksAccessibleByMouse)
        self.document().setDocumentMargin(0)
        self.setHtml(MarkdownToHtml(markdown, compact=True))
        self._updateHeight()

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._updateHeight()

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        super().showEvent(event)
        QtCore.QTimer.singleShot(0, self._updateHeight)

    def _updateHeight(self) -> None:
        width = self.viewport().width()
        if width > 0:
            self.document().setTextWidth(width)
        docHeight = self.document().size().height()
        self.setFixedHeight(max(1, int(docHeight)))


class _MessageBubble(QtWidgets.QFrame):
    def __init__(self, text: str, isAi: bool = False, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setObjectName("aiBubble" if isAi else "userBubble")
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(6, 4, 6, 4)
        layout.setSpacing(8)

        if isAi:
            avatar = QtWidgets.QLabel()
            avatar.setFixedSize(28, 28)
            avatar.setAlignment(QtCore.Qt.AlignCenter)
            avatar.setStyleSheet("background: #4a90d9; color: white; border-radius: 14px; font-weight: bold; font-size: 11px;")
            avatar.setText("AI")
            layout.addWidget(avatar, 0, QtCore.Qt.AlignTop)

        bubbleStyle = (
            "background: #2d2d2d; color: #e0e0e0; border-radius: 8px; padding: 6px 10px;"
            if isAi else
            "background: #1a5fb4; color: white; border-radius: 8px; padding: 6px 10px;"
        )
        contentHost = QtWidgets.QWidget()
        contentHost.setStyleSheet(bubbleStyle)
        contentLayout = QtWidgets.QVBoxLayout(contentHost)
        contentLayout.setContentsMargins(0, 0, 0, 0)
        contentLayout.setSpacing(0)

        if isAi:
            content = _MarkdownBubbleContent(text, contentHost)
        else:
            content = QtWidgets.QLabel(text)
            content.setWordWrap(True)
            content.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
            content.setStyleSheet("background: transparent; border: none;")
        contentLayout.addWidget(content)
        layout.addWidget(contentHost, 1)

        if not isAi:
            layout.addStretch()
        else:
            layout.addStretch()


class AiChatDialog(QtWidgets.QDialog):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None,
                 blueprintName: str = "", blueprintFilePath: str = "") -> None:
        super().__init__(parent)
        self._messages: list[tuple[str, str]] = []
        self._loading: bool = False
        self._quickjsContext: Any = None
        self._agentJsMtimeKey: Optional[str] = None
        self._blueprintName = blueprintName
        self._blueprintFilePath = blueprintFilePath
        self._setupUi()
        self._resetLogFile()
        self._ensureAgentJs()

    def _setupUi(self) -> None:
        title = ELOC("AI_CHAT_TITLE")
        if self._blueprintName:
            title = f"{title} - {self._blueprintName}"
        self.setWindowTitle(title)
        self.resize(600, 500)

        mainLayout = QtWidgets.QVBoxLayout(self)
        mainLayout.setContentsMargins(0, 0, 0, 0)
        mainLayout.setSpacing(0)

        headerBar = QtWidgets.QWidget()
        headerBar.setFixedHeight(36)
        headerBar.setStyleSheet("background: #1e1e1e; border-bottom: 1px solid #333;")
        headerLayout = QtWidgets.QHBoxLayout(headerBar)
        headerLayout.setContentsMargins(8, 4, 8, 4)

        self._settingsBtn = QtWidgets.QPushButton(ELOC("AI_SETTINGS"))
        self._settingsBtn.setFixedSize(80, 24)
        self._settingsBtn.setAutoDefault(False)
        self._settingsBtn.setStyleSheet("QPushButton { background: #333; color: #ccc; border: 1px solid #555; border-radius: 4px; padding: 2px 8px; } QPushButton:hover { background: #444; }")
        self._settingsBtn.clicked.connect(self._openSettings)
        headerLayout.addWidget(self._settingsBtn)
        headerLayout.addStretch()
        mainLayout.addWidget(headerBar)

        self._scrollArea = QtWidgets.QScrollArea()
        self._scrollArea.setWidgetResizable(True)
        self._scrollArea.setFrameShape(QtWidgets.QFrame.NoFrame)
        self._scrollArea.setStyleSheet("QScrollArea { background: #121212; }")

        self._msgContainer = QtWidgets.QWidget()
        self._msgLayout = QtWidgets.QVBoxLayout(self._msgContainer)
        self._msgLayout.setContentsMargins(8, 8, 8, 8)
        self._msgLayout.setSpacing(6)
        self._msgLayout.addStretch()
        self._scrollArea.setWidget(self._msgContainer)
        mainLayout.addWidget(self._scrollArea, 1)

        self._stepStatusLabel = QtWidgets.QLabel()
        self._stepStatusLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self._stepStatusLabel.setWordWrap(True)
        self._stepStatusLabel.setStyleSheet("color: #666; padding: 2px 12px 0 12px; font-size: 11px;")
        self._stepStatusLabel.hide()
        mainLayout.addWidget(self._stepStatusLabel)

        self._loadingLabel = QtWidgets.QLabel()
        self._loadingLabel.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        self._loadingLabel.setStyleSheet("color: #888; padding: 4px 12px; font-style: italic;")
        self._loadingLabel.hide()
        mainLayout.addWidget(self._loadingLabel)

        inputBar = QtWidgets.QWidget()
        inputBar.setStyleSheet("background: #1e1e1e; border-top: 1px solid #333;")
        inputLayout = QtWidgets.QHBoxLayout(inputBar)
        inputLayout.setContentsMargins(8, 6, 8, 6)
        inputLayout.setSpacing(6)

        self._inputEdit = QtWidgets.QLineEdit()
        self._inputEdit.setPlaceholderText(ELOC("AI_INPUT_HINT"))
        self._inputEdit.setStyleSheet("QLineEdit { background: #2a2a2a; color: #e0e0e0; border: 1px solid #444; border-radius: 4px; padding: 6px 10px; }")
        self._inputEdit.returnPressed.connect(self._onSend)
        inputLayout.addWidget(self._inputEdit, 1)

        self._sendBtn = QtWidgets.QPushButton(ELOC("SEND"))
        self._sendBtn.setFixedSize(60, 30)
        self._sendBtn.setDefault(True)
        self._sendBtn.setStyleSheet("QPushButton { background: #1a5fb4; color: white; border: none; border-radius: 4px; } QPushButton:hover { background: #2272d4; } QPushButton:disabled { background: #444; color: #888; }")
        self._sendBtn.clicked.connect(self._onSend)
        inputLayout.addWidget(self._sendBtn)

        mainLayout.addWidget(inputBar)

        self._loadingTimer = QtCore.QTimer(self)
        self._loadingTimer.timeout.connect(self._updateLoadingDots)
        self._loadingDots = 0

    def _createQuickJsContext(self) -> Any:
        import quickjs

        ctx = quickjs.Context()
        ctx.add_callable("pyFetch", self._pyFetch)
        ctx.add_callable("pyBuildFileTree", self._pyBuildFileTree)
        ctx.add_callable("pyRunTerminal", self._pyRunTerminal)
        ctx.add_callable("pyFetchFile", self._pyFetchFile)
        ctx.add_callable("pyReadFile", self._pyReadFile)
        ctx.add_callable("pyReadAgentResource", self._pyReadAgentResource)
        ctx.add_callable("pyGetAgentTools", self._pyGetAgentTools)
        ctx.add_callable("pyReplaceFile", self._pyReplaceFile)
        ctx.add_callable("pyPatchBlueprint", self._pyPatchBlueprint)
        ctx.add_callable("pyValidateBlueprint", self._pyValidateBlueprint)
        ctx.add_callable("pyReportStatus", self._pyReportStatus)
        ctx.add_callable("pyLog", self._pyLog)
        return ctx

    def _loadAgentJs(self) -> None:
        from agent.PromptResources import GetAgentJsBundleMtimeKey, GetAgentJsRelPaths

        if self._quickjsContext is None:
            return
        rootPath = File.GetRootPath()
        for jsFile in GetAgentJsRelPaths():
            jsPath = os.path.join(rootPath, jsFile.replace("/", os.sep))
            if os.path.isfile(jsPath):
                with open(jsPath, "r", encoding="utf-8") as handle:
                    self._quickjsContext.eval(handle.read())
        self._agentJsMtimeKey = GetAgentJsBundleMtimeKey()

    def _ensureAgentJs(self) -> None:
        from agent.PromptResources import GetAgentJsBundleMtimeKey

        _ensure_log_handler()
        mtimeKey = GetAgentJsBundleMtimeKey()
        if self._quickjsContext is None:
            try:
                self._quickjsContext = self._createQuickJsContext()
                self._loadAgentJs()
            except Exception as e:
                _agent_log("QuickJS init failed: %s", e)
                self._quickjsContext = None
                self._agentJsMtimeKey = None
            return
        if mtimeKey != self._agentJsMtimeKey:
            log.info("Agent JS bundle changed, reloading QuickJS context")
            try:
                self._quickjsContext = self._createQuickJsContext()
                self._loadAgentJs()
            except Exception as e:
                _agent_log("QuickJS reload failed: %s", e)
                self._quickjsContext = None
                self._agentJsMtimeKey = None

    def _pyReadAgentResource(self, relPath: str) -> str:
        from agent.PromptResources import LoadAgentResource

        return LoadAgentResource(relPath)

    def _pyGetAgentTools(self) -> str:
        from agent.PromptResources import LoadAgentTools

        return LoadAgentTools()

    def _resetLogFile(self) -> None:
        if System.AlreadyPacked():
            return
        root = File.GetRootPath()
        if not root:
            return
        _ensure_log_handler()
        log_path = os.path.join(root, "LudorkAI.log")
        with open(log_path, "w", encoding="utf-8") as f:
            f.write("")
        _append_ludork_ai_log("=== LudorkAI.log reset ===")

    def _pyFetch(self, url: str, apiKey: str, body: str) -> str:
        data = body.encode("utf-8")
        req = urllib.request.Request(url + "/chat/completions", data=data)
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Bearer {apiKey}")
        try:
            with urllib.request.urlopen(req, timeout=120) as resp:
                return resp.read().decode("utf-8")
        except Exception as e:
            return json.dumps({"error": str(e)})

    def _pyLog(self, msg: str) -> None:
        _agent_log(msg)

    def _pyReportStatus(self, payloadJson: str) -> None:
        QtCore.QMetaObject.invokeMethod(
            self,
            "_onAgentStatus",
            QtCore.Qt.QueuedConnection,
            QtCore.Q_ARG(str, payloadJson),
        )

    @QtCore.pyqtSlot(str)
    def _onAgentStatus(self, payloadJson: str) -> None:
        if not self._loading:
            return
        try:
            payload = json.loads(payloadJson)
        except json.JSONDecodeError:
            return
        if not isinstance(payload, dict):
            return
        text = self._formatAgentStatus(payload)
        if text:
            self._stepStatusLabel.setText(text)
            self._stepStatusLabel.show()

    def _formatAgentStatus(self, payload: dict[str, Any]) -> str:
        statusKey = payload.get("key", "")
        detail = str(payload.get("detail", "") or "")
        iteration = payload.get("iteration", 0)
        prefix = ""
        try:
            iterationValue = int(iteration)
            if iterationValue > 0:
                prefix = ELOC("AI_STATUS_ITERATION").format(n=iterationValue) + " · "
        except (TypeError, ValueError):
            pass
        if statusKey == "iteration":
            try:
                return ELOC("AI_STATUS_ITERATION").format(n=int(iteration))
            except (TypeError, ValueError):
                return ELOC("AI_STATUS_ITERATION").format(n=0)
        if statusKey == "calling_api":
            return prefix + ELOC("AI_STATUS_CALLING_API")
        if statusKey == "fileprocess":
            return prefix + ELOC("AI_STATUS_FILE_PROCESS").format(detail=detail)
        if statusKey == "fetchfile":
            return prefix + ELOC("AI_STATUS_FETCH_FILE").format(detail=detail)
        if statusKey == "readfile":
            return prefix + ELOC("AI_STATUS_READ_FILE").format(detail=detail)
        if statusKey == "replacefile":
            return prefix + ELOC("AI_STATUS_REPLACE_FILE")
        if statusKey == "patchblueprint":
            return prefix + ELOC("AI_STATUS_PATCH_BLUEPRINT")
        if statusKey == "validate":
            return prefix + ELOC("AI_STATUS_VALIDATE")
        if statusKey == "format_retry":
            return prefix + ELOC("AI_STATUS_FORMAT_RETRY")
        if statusKey == "continue_retry":
            return prefix + ELOC("AI_STATUS_CONTINUE_RETRY")
        return ""

    def _pyBuildFileTree(self) -> str:
        try:
            from agent.FileTree import BuildAgentFileTree
            proj = EditorStatus.PROJ_PATH
            if not proj or not os.path.isdir(proj):
                return "(no project open)"
            return BuildAgentFileTree(proj)
        except Exception as e:
            return f"Error building file tree: {e}"

    def _pyRunTerminal(self, command: str, timeout: int = 120) -> str:
        try:
            from agent.Terminal import RunTerminal
            proj = EditorStatus.PROJ_PATH or os.getcwd()
            return RunTerminal(command, proj, timeout)
        except Exception as e:
            return f"Error running terminal: {e}"

    def _pyFetchFile(self, keyword: str) -> str:
        results: list[str] = []
        keyword_lower = keyword.strip().lower()

        blueprints_data = getattr(GameData, "blueprintsData", {})
        for key, data in blueprints_data.items():
            data_str = json.dumps(data, ensure_ascii=False, default=str)
            if keyword_lower and keyword_lower in data_str.lower():
                idx = data_str.lower().find(keyword_lower)
                start = max(0, idx - 40)
                end = min(len(data_str), idx + len(keyword) + 40)
                snippet = data_str[start:end]
                results.append(f'Blueprint "{key}" (around offset {idx}): ...{snippet}...')
            if keyword_lower and keyword_lower in key.lower():
                results.append(f'Blueprint key match: "{key}"')

        try:
            from agent.ProjectSearch import SearchProjectKeyword
            proj = EditorStatus.PROJ_PATH or ""
            source_result = SearchProjectKeyword(proj, keyword)
            if source_result and not source_result.startswith("Error:"):
                results.append(source_result)
        except Exception as exc:
            results.append(f"Source search error: {exc}")

        if not results:
            return f'No matches found for "{keyword}"'
        return "\n".join(results[:30])

    def _pyReadFile(self, relPath: str) -> str:
        try:
            from agent.ProjectSearch import ReadProjectFile
            proj = EditorStatus.PROJ_PATH or ""
            return ReadProjectFile(proj, relPath)
        except Exception as exc:
            return f"Error reading file: {exc}"

    def _pyValidateBlueprint(self) -> str:
        try:
            data = None
            if self._blueprintName and hasattr(GameData, "blueprintsData"):
                loaded = GameData.blueprintsData.get(self._blueprintName)
                if isinstance(loaded, dict):
                    data = loaded
            valid, errors = ValidateBlueprint(self._blueprintName, data)
            return json.dumps({"valid": valid, "errors": errors}, ensure_ascii=False)
        except Exception as e:
            return json.dumps({"valid": False, "errors": [str(e)]}, ensure_ascii=False)

    def _getCurrentBlueprintData(self) -> Optional[dict[str, Any]]:
        if self._blueprintName and hasattr(GameData, "blueprintsData"):
            loaded = GameData.blueprintsData.get(self._blueprintName)
            if isinstance(loaded, dict):
                return loaded
        if not self._blueprintFilePath or not os.path.exists(self._blueprintFilePath):
            return None
        try:
            ext = os.path.splitext(self._blueprintFilePath)[1].lower()
            if ext == ".dat":
                with open(self._blueprintFilePath, "rb") as f:
                    loaded = pickle.load(f)
            else:
                with open(self._blueprintFilePath, "r", encoding="utf-8") as f:
                    loaded = json.load(f)
            if isinstance(loaded, dict):
                return loaded
        except Exception:
            return None
        return None

    def _parseReplacementJson(self, newContentJson: str) -> tuple[Optional[dict[str, Any]], Optional[str]]:
        try:
            parsed = json.loads(newContentJson)
            if isinstance(parsed, dict):
                return parsed, None
            return None, "Error: Blueprint replacement data must be a JSON object"
        except json.JSONDecodeError as exc:
            if "Extra data" not in str(exc):
                return None, f"Error: Invalid JSON in replacement data: {exc}"
            decoder = JSONDecoder()
            try:
                partial, _end = decoder.raw_decode(newContentJson.lstrip())
            except json.JSONDecodeError as inner_exc:
                return None, f"Error: Invalid JSON in replacement data: {inner_exc}"
            if not isinstance(partial, dict):
                return None, "Error: Blueprint replacement data must be a JSON object"
            base = self._getCurrentBlueprintData()
            if base is None:
                return None, f"Error: Invalid JSON in replacement data: {exc}"
            from agent.BlueprintPatch import MergeBlueprintWithBase

            merged = MergeBlueprintWithBase(partial, base)
            log.info("Merged partial replacefile JSON with current blueprint (%d chars input)", len(newContentJson))
            return merged, None

    def _saveBlueprintData(self, newData: dict[str, Any]) -> str:
        if not self._blueprintFilePath:
            return "Error: No blueprint file path available for replacement"
        parentDir = os.path.dirname(self._blueprintFilePath)
        if not parentDir or not os.path.isdir(parentDir):
            return "Error: No blueprint file path available for replacement"

        from agent.BlueprintPatch import NormalizeBlueprintForSave

        normalized = NormalizeBlueprintForSave(newData)
        try:
            normalized = GameData._GetBlueprintSavePayload(normalized)
        except Exception:
            pass
        valid, errors = ValidateBlueprint(self._blueprintName, normalized)
        if not valid:
            detail = "\n".join(errors) if errors else "Unknown validation error"
            return f"Error: Blueprint validation failed:\n{detail}"

        try:
            GameData.RecordSnapshot()
            ext = os.path.splitext(self._blueprintFilePath)[1].lower()
            if ext == ".dat":
                with open(self._blueprintFilePath, "wb") as f:
                    pickle.dump(normalized, f)
            else:
                with open(self._blueprintFilePath, "w", encoding="utf-8") as f:
                    json.dump(normalized, f, ensure_ascii=False, indent=4)

            if self._blueprintName:
                GameData.ApplyBlueprintFileUpdate(self._blueprintName, self._blueprintFilePath)

            QtCore.QMetaObject.invokeMethod(
                self,
                "_refreshBlueprintEditorUi",
                QtCore.Qt.QueuedConnection,
            )

            return f"Modified: {self._blueprintFilePath}\nBlueprint \"{self._blueprintName}\" saved successfully."
        except Exception as e:
            log.exception("Blueprint save failed for %s", self._blueprintName)
            return f"Error saving blueprint: {e}"

    def _pyReplaceFile(self, newContentJson: str) -> str:
        newData, parse_error = self._parseReplacementJson(newContentJson)
        if parse_error is not None:
            return parse_error
        if newData is None:
            return "Error: Blueprint replacement data must be a JSON object"
        base_data = self._getCurrentBlueprintData()
        if base_data is not None:
            from agent.BlueprintPatch import MergeBlueprintWithBase

            newData = MergeBlueprintWithBase(newData, base_data)
        return self._saveBlueprintData(newData)

    def _pyPatchBlueprint(self, opsJson: str) -> str:
        try:
            ops = json.loads(opsJson)
        except json.JSONDecodeError as exc:
            return f"Error: Invalid JSON in patch operations: {exc}"

        if not isinstance(ops, list):
            return "Error: Patch operations must be a JSON array"

        base_data = self._getCurrentBlueprintData()
        if base_data is None:
            return "Error: No blueprint data available for patching"

        from agent.BlueprintPatch import ApplyBlueprintPatches

        patched, patch_errors = ApplyBlueprintPatches(base_data, ops)
        if patch_errors:
            return "Error: Blueprint patch failed:\n" + "\n".join(patch_errors)
        if patched is None:
            return "Error: Blueprint patch failed with unknown error"
        return self._saveBlueprintData(patched)

    @QtCore.pyqtSlot()
    def _refreshBlueprintEditorUi(self) -> None:
        parent = self.parent()
        applyUpdate = getattr(parent, "applyExternalBlueprintUpdate", None)
        if callable(applyUpdate):
            applyUpdate()

    def _getBlueprintDataJson(self) -> str:
        data = self._getCurrentBlueprintData()
        if data is None:
            return ""
        try:
            from agent.BlueprintPatch import CompactBlueprintForAgent

            compact = CompactBlueprintForAgent(data)
            return json.dumps(compact, ensure_ascii=False, separators=(",", ":"))
        except Exception:
            return ""

    def _getParentClass(self) -> str:
        if not self._blueprintName:
            return ""
        bp = getattr(GameData, "blueprintsData", {}).get(self._blueprintName)
        if isinstance(bp, dict):
            return str(bp.get("parent", ""))
        return ""

    def _openSettings(self) -> None:
        dlg = AiConfigDialog(self)
        dlg.exec_()

    def _onSend(self) -> None:
        text = self._inputEdit.text().strip()
        if not text or self._loading:
            return
        self._inputEdit.clear()
        self._addMessage("user", text)
        self._startLoading()
        threading.Thread(target=self._callAi, args=(text,), daemon=True).start()

    def _addMessage(self, role: str, content: str) -> None:
        isAi = role != "user"
        bubble = _MessageBubble(content, isAi)
        idx = self._msgLayout.count() - 1
        self._msgLayout.insertWidget(max(0, idx), bubble)
        self._messages.append((role, content))
        line = f"[Conversation] {role}: {content}"
        _agent_log(line)

    def _startLoading(self) -> None:
        self._loading = True
        self._loadingDots = 0
        self._stepStatusLabel.setText(ELOC("AI_STATUS_STARTING"))
        self._stepStatusLabel.show()
        self._loadingLabel.show()
        self._loadingTimer.start(400)
        self._sendBtn.setEnabled(False)
        self._inputEdit.setEnabled(False)

    def _stopLoading(self) -> None:
        self._loading = False
        self._loadingTimer.stop()
        self._loadingLabel.hide()
        self._stepStatusLabel.hide()
        self._sendBtn.setEnabled(True)
        self._inputEdit.setEnabled(True)
        self._inputEdit.setFocus()

    def _updateLoadingDots(self) -> None:
        self._loadingDots = (self._loadingDots + 1) % 4
        dots = "." * (self._loadingDots + 1)
        self._loadingLabel.setText(ELOC("AI_THINKING") + dots)

    def _callAi(self, userInput: str) -> None:
        result = ""
        provider = GetAiProvider()
        model = GetAiModel()
        apiKey = GetAiApiKey()
        baseUrl = GetAiBaseUrl()

        _agent_log(
            "=== _callAi start | blueprint=%s | userInput=%s | provider=%s | model=%s",
            self._blueprintName, userInput, provider, model,
        )

        if not provider or not model or not apiKey:
            result = ELOC("API_KEY_NOT_CONFIGURED")

        self._ensureAgentJs()

        if not result and self._quickjsContext is not None:
            try:
                intent = _classify_intent(userInput, provider, model, apiKey, baseUrl)
                _agent_log("Intent classification: %s", intent)

                if intent == "general_query":
                    result = _answer_general_query(userInput, provider, model, apiKey, baseUrl)
                    if not result:
                        _agent_log("General query direct answer returned empty, routing to main workflow")
                        intent = "blueprint_query"
                    else:
                        _agent_log("General query direct answer: %d chars", len(result))
                        QtCore.QMetaObject.invokeMethod(
                            self, "_onAiResponse", QtCore.Qt.QueuedConnection, QtCore.Q_ARG(str, result)
                        )
                        return

                expectsModification = (intent == "modify")
                systemPrompt = _build_full_system_prompt(
                    self._blueprintName,
                    self._getParentClass(),
                    EditorStatus.PROJ_PATH or "",
                )
                blueprintDataJson = self._getBlueprintDataJson()
                fileTree = self._pyBuildFileTree()

                _agent_log(
                    "systemPrompt=%d chars, fileTree=%d chars, blueprintData=%d chars",
                    len(systemPrompt), len(fileTree), len(blueprintDataJson),
                )

                contextMessages: list[dict[str, str]] = []
                msgs = list(self._messages)
                if msgs and msgs[-1][0] == "user":
                    msgs = msgs[:-1]
                for role, content in msgs:
                    contextMessages.append({"role": "assistant" if role == "ai" else role, "content": content})

                if len(contextMessages) > 20:
                    _agent_log("Compressing %d context messages...", len(contextMessages))
                    jsCode = f"compressContext({json.dumps(provider)}, {json.dumps(model)}, {json.dumps(apiKey)}, {json.dumps(baseUrl)}, {json.dumps(json.dumps(contextMessages))})"
                    summary = cast(str, self._quickjsContext.eval(jsCode))
                    if summary:
                        contextMessages = [{"role": "system", "content": summary}]
                        _agent_log("Compressed to %d chars summary", len(summary))

                contextJson = json.dumps(contextMessages)
                jsCode = (
                    f"runWorkflow("
                    f"{json.dumps(provider)}, "
                    f"{json.dumps(model)}, "
                    f"{json.dumps(apiKey)}, "
                    f"{json.dumps(baseUrl)}, "
                    f"{json.dumps(systemPrompt)}, "
                    f"{json.dumps(fileTree)}, "
                    f"{json.dumps(blueprintDataJson)}, "
                    f"{json.dumps(userInput)}, "
                    f"{json.dumps(contextJson)}, "
                    f"{json.dumps(expectsModification)}"
                    f")"
                )
                _agent_log("Calling runWorkflow (jsCode=%d chars)", len(jsCode))
                result = cast(str, self._quickjsContext.eval(jsCode))
                _agent_log("runWorkflow result: %s", result)
            except Exception as e:
                result = str(e)

        if not result:
            result = "AI agent not available. Ensure the 'quickjs' module is installed and Start.js/Compress.js are accessible."

        QtCore.QMetaObject.invokeMethod(
            self, "_onAiResponse", QtCore.Qt.QueuedConnection, QtCore.Q_ARG(str, result)
        )

    @QtCore.pyqtSlot(str)
    def _onAiResponse(self, content: str) -> None:
        self._stopLoading()
        self._addMessage("ai", content)
        scrollbar = self._scrollArea.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())
