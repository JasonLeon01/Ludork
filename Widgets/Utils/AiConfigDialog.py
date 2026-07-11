# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal import EditorStatus
from EditorGlobal.QmlDialogHost import QmlDialogHost
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
    cfg = EditorStatus.EDITOR_CONFIG
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
        EditorStatus.EDITOR_CONFIG.write(f)


class AiConfigDialog(QmlDialogHost):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(
            parent,
            ELOC("ENTER_API_KEY"),
            QtCore.QSize(272, 188),
            formLabels=(ELOC("AI_PROVIDER"), ELOC("AI_MODEL"), ELOC("API_KEY")),
        )
        self.loadQml(
            "Dialogs/AiConfigDialog.qml",
            {
                "aiProviders": list(_PROVIDERS),
                "aiProviderModels": {key: list(value) for key, value in _PROVIDER_MODELS.items()},
                "aiCurrentProvider": GetAiProvider(),
                "aiCurrentModel": GetAiModel(),
                "aiCurrentApiKey": GetAiApiKey(),
            },
        )

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        provider = str(result.get("provider", "")).strip()
        model = str(result.get("model", "")).strip()
        apiKey = str(result.get("apiKey", "")).strip()
        if not provider or not model or not apiKey:
            return False
        _saveConfig(provider, model, apiKey)
        return True
