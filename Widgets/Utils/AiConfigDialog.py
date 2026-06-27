# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import Optional
from PyQt5 import QtWidgets
from EditorGlobal import EditorStatus
from Utils import File

_PROVIDERS = ["OpenAI", "DeepSeek", "Google", "Anthropic", "Custom"]

_PROVIDER_MODELS: dict[str, list[str]] = {
    "OpenAI": ["gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "o3-mini", "o1"],
    "DeepSeek": ["deepseek-v4-pro", "deepseek-v4-flash"],
    "Google": ["gemini-2.5-flash", "gemini-2.5-pro", "gemini-2.0-flash"],
    "Anthropic": ["claude-sonnet-4-20250514", "claude-haiku-3-5-202510"],
    "Custom": [],
}

_PROVIDER_BASE_URLS: dict[str, str] = {
    "OpenAI": "https://api.openai.com/v1",
    "DeepSeek": "https://api.deepseek.com",
    "Google": "https://generativelanguage.googleapis.com/v1beta/openai/",
    "Anthropic": "",
    "Custom": "",
}


def _section() -> str:
    return EditorStatus.APP_NAME


def _config():
    cfg = EditorStatus.editorConfig
    if _section() not in cfg:
        cfg[_section()] = {}
    return cfg[_section()]


def IsAiConfigured() -> bool:
    sec = _config()
    return bool(str(sec.get("aiprovider", "")).strip()) and bool(str(sec.get("apikey", "")).strip())


def GetAiProvider() -> str:
    return str(_config().get("aiprovider", "")).strip()


def GetAiModel() -> str:
    return str(_config().get("aimodel", "")).strip()


def GetAiApiKey() -> str:
    return str(_config().get("apikey", "")).strip()


def GetAiBaseUrl() -> str:
    provider = GetAiProvider()
    return _PROVIDER_BASE_URLS.get(provider, "")


def _saveConfig(provider: str, model: str, apiKey: str) -> None:
    sec = _config()
    sec["aiprovider"] = provider
    sec["aimodel"] = model
    sec["apikey"] = apiKey
    cfgPath = os.path.join(File.GetIniPath(), f"{_section()}.ini")
    with open(cfgPath, "w", encoding="utf-8") as f:
        EditorStatus.editorConfig.write(f)


class AiConfigDialog(QtWidgets.QDialog):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._setupUi()

    def _setupUi(self) -> None:
        self.setWindowTitle(ELOC("ENTER_API_KEY"))
        form = QtWidgets.QFormLayout(self)
        form.setContentsMargins(12, 12, 12, 12)
        form.setSpacing(8)

        self._providerCombo = QtWidgets.QComboBox(self)
        self._providerCombo.setEditable(False)
        self._providerCombo.addItems(_PROVIDERS)
        currentProvider = GetAiProvider()
        if currentProvider in _PROVIDERS:
            idx = _PROVIDERS.index(currentProvider)
            self._providerCombo.setCurrentIndex(idx)
        form.addRow(ELOC("AI_PROVIDER"), self._providerCombo)

        self._modelCombo = QtWidgets.QComboBox(self)
        self._modelCombo.setEditable(False)
        form.addRow(ELOC("AI_MODEL"), self._modelCombo)

        self._keyEdit = QtWidgets.QLineEdit(self)
        self._keyEdit.setPlaceholderText(ELOC("API_KEY"))
        self._keyEdit.setText(GetAiApiKey())
        form.addRow(ELOC("API_KEY"), self._keyEdit)

        btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        self._okBtn = btns.button(QtWidgets.QDialogButtonBox.Ok)
        cancelBtn = btns.button(QtWidgets.QDialogButtonBox.Cancel)
        if self._okBtn:
            self._okBtn.setText(ELOC("CONFIRM"))
        if cancelBtn:
            cancelBtn.setText(ELOC("CANCEL"))
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        form.addRow(btns)

        self._refreshModels()
        self._providerCombo.currentTextChanged.connect(self._onProviderChanged)
        self._providerCombo.currentTextChanged.connect(self._validate)
        self._modelCombo.currentTextChanged.connect(self._validate)
        self._keyEdit.textChanged.connect(self._validate)
        self._validate()

    def _onProviderChanged(self, _text: str) -> None:
        self._refreshModels()

    def _refreshModels(self) -> None:
        provider = self._providerCombo.currentText().strip()
        models = _PROVIDER_MODELS.get(provider, [])
        self._modelCombo.clear()
        if models:
            self._modelCombo.addItems(models)
        currentModel = GetAiModel()
        if currentModel:
            idx = self._modelCombo.findText(currentModel)
            if idx >= 0:
                self._modelCombo.setCurrentIndex(idx)

    def _validate(self) -> None:
        if self._okBtn:
            valid = bool(self._providerCombo.currentText().strip()) and bool(self._modelCombo.currentText().strip()) and bool(self._keyEdit.text().strip())
            self._okBtn.setEnabled(valid)

    def accept(self) -> None:
        provider = self._providerCombo.currentText().strip()
        model = self._modelCombo.currentText().strip()
        key = self._keyEdit.text().strip()
        _saveConfig(provider, model, key)
        super().accept()
