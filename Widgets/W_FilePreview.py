# -*- encoding: utf-8 -*-

from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets, QtMultimedia
from Utils import Locale, Panel


class FilePreview(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._path = ""
        self._origPixmap = None
        self._stack = QtWidgets.QStackedWidget(self)
        self._imageLabel = QtWidgets.QLabel(self)
        self._imageLabel.setAlignment(QtCore.Qt.AlignCenter)
        self._audioWidget = QtWidgets.QWidget(self)
        self._player = QtMultimedia.QMediaPlayer(self)
        self._playButton = QtWidgets.QToolButton(self)
        self._playButton.setText("▶")
        self._positionSlider = QtWidgets.QSlider(QtCore.Qt.Horizontal, self)
        self._positionSlider.setRange(0, 0)
        self._timeLabel = QtWidgets.QLabel("00:00 / 00:00", self)
        self._openSystemBtn = QtWidgets.QToolButton(self)
        self._openSystemBtn.setText(Locale.getContent("OPEN_FROM_SYSTEM"))
        self._audioErrorLabel = QtWidgets.QLabel("", self)
        self._audioErrorLabel.setStyleSheet("color: #f0a0a0;")
        bar = QtWidgets.QHBoxLayout()
        bar.setContentsMargins(0, 0, 0, 0)
        bar.setSpacing(6)
        bar.addWidget(self._playButton, 0)
        bar.addWidget(self._positionSlider, 1)
        bar.addWidget(self._timeLabel, 0)
        err = QtWidgets.QHBoxLayout()
        err.setContentsMargins(0, 0, 0, 0)
        err.setSpacing(6)
        err.addWidget(self._audioErrorLabel, 1)
        err.addWidget(self._openSystemBtn, 0)
        v = QtWidgets.QVBoxLayout()
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(6)
        v.addLayout(bar)
        v.addLayout(err)
        self._audioWidget.setLayout(v)
        self._stack.addWidget(self._imageLabel)
        self._stack.addWidget(self._audioWidget)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._stack, 1)
        self.setMinimumHeight(160)
        self._playButton.clicked.connect(self._onTogglePlay)
        self._player.durationChanged.connect(self._onDurationChanged)
        self._player.positionChanged.connect(self._onPositionChanged)
        self._player.stateChanged.connect(self._onStateChanged)
        self._player.mediaStatusChanged.connect(self._onMediaStatusChanged)
        try:
            self._player.error.connect(self._onError)
        except Exception:
            pass
        self._positionSlider.sliderMoved.connect(self._onSeek)
        self._openSystemBtn.clicked.connect(self._openSystem)
        Panel.applyDisabledOpacity(self)
    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.applyDisabledOpacity(self)
        super().changeEvent(e)

    def setFile(self, path: str) -> None:
        self._stopAudio()
        self._path = path or ""
        ext = self._suffix(self._path)
        if ext in {"png", "jpg", "jpeg", "bmp", "gif", "webp"}:
            self._showImage(self._path)
        elif ext in {"mp3", "wav", "ogg", "flac", "aac", "m4a"}:
            self._showAudio(self._path)
        else:
            self._clearViews()

    def clear(self) -> None:
        self._path = ""
        self._clearViews()

    def _suffix(self, p: str) -> str:
        i = p.rfind(".")
        return p[i + 1 :].lower() if i >= 0 else ""

    def _clearViews(self) -> None:
        self._imageLabel.clear()
        self._origPixmap = None
        self._stack.setCurrentWidget(self._imageLabel)

    def _showImage(self, p: str) -> None:
        pm = QtGui.QPixmap(p)
        self._origPixmap = pm if not pm.isNull() else None
        self._updateImagePixmap()
        self._stack.setCurrentWidget(self._imageLabel)

    def _showAudio(self, p: str) -> None:
        self._stack.setCurrentWidget(self._audioWidget)
        self._playButton.setEnabled(True)
        self._positionSlider.setEnabled(True)
        self._setAudioError("")
        url = QtCore.QUrl.fromLocalFile(p)
        self._player.setMedia(QtMultimedia.QMediaContent(url))
        try:
            self._player.setVolume(100)
        except Exception:
            pass
        self._player.pause()
        self._playButton.setText("▶")

    def _updateImagePixmap(self) -> None:
        if self._origPixmap is None:
            self._imageLabel.clear()
            return
        if self._imageLabel.width() <= 0 or self._imageLabel.height() <= 0:
            return
        scaled = self._origPixmap.scaled(
            self._imageLabel.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation
        )
        self._imageLabel.setPixmap(scaled)

    def resizeEvent(self, e: QtGui.QResizeEvent) -> None:
        super().resizeEvent(e)
        self._updateImagePixmap()

    def _onTogglePlay(self) -> None:
        if self._player.state() == QtMultimedia.QMediaPlayer.PlayingState:
            self._player.pause()
        else:
            self._player.play()

    def _onDurationChanged(self, d: int) -> None:
        self._positionSlider.setRange(0, d if d > 0 else 0)
        self._updateTimeLabel(self._player.position(), d)

    def _onPositionChanged(self, p: int) -> None:
        if not self._positionSlider.isSliderDown():
            self._positionSlider.setValue(p)
        self._updateTimeLabel(p, self._player.duration())

    def _onSeek(self, v: int) -> None:
        self._player.setPosition(v)

    def _onStateChanged(self, s: QtMultimedia.QMediaPlayer.State) -> None:
        self._playButton.setText("⏸" if s == QtMultimedia.QMediaPlayer.PlayingState else "▶")

    def _onMediaStatusChanged(self, st: QtMultimedia.QMediaPlayer.MediaStatus) -> None:
        if st == QtMultimedia.QMediaPlayer.InvalidMedia:
            self._playButton.setEnabled(False)
            self._positionSlider.setEnabled(False)
            self._setAudioError(Locale.getContent("PLAY_ERROR"))

    def _onError(self, err) -> None:
        if self._path:
            try:
                emsg = self._player.errorString()
            except Exception:
                emsg = ""
            self._setAudioError(Locale.getContent("PLAY_ERROR") or emsg)

    def _stopAudio(self) -> None:
        try:
            self._player.stop()
        except Exception:
            pass

    def _formatMs(self, ms: int) -> str:
        if ms <= 0:
            return "00:00"
        sec = ms // 1000
        m = sec // 60
        s = sec % 60
        return f"{m:02d}:{s:02d}"

    def _updateTimeLabel(self, pos: int, dur: int) -> None:
        self._timeLabel.setText(f"{self._formatMs(pos)} / {self._formatMs(dur)}")

    def _setAudioError(self, text: str) -> None:
        show = bool(text)
        self._audioErrorLabel.setText(text)
        self._audioErrorLabel.setVisible(show)
        self._openSystemBtn.setVisible(show)

    def _openSystem(self) -> None:
        if not self._path:
            return
        QtGui.QDesktopServices.openUrl(QtCore.QUrl.fromLocalFile(self._path))

    def _audioMimeForPath(self, p: str) -> str:
        ext = self._suffix(p)
        if ext == "mp3":
            return "audio/mpeg"
        if ext == "wav":
            return "audio/wav"
        if ext == "ogg":
            return "audio/ogg"
        if ext == "flac":
            return "audio/flac"
        if ext == "aac":
            return "audio/aac"
        if ext == "m4a":
            return "audio/mp4"
        return ""

    def _isAudioSupported(self, p: str) -> bool:
        try:
            m = self._audioMimeForPath(p)
            if not m:
                return True
            sup = getattr(QtMultimedia, "QMultimedia", None)
            if sup is None:
                return True
            mts = sup.supportedMimeTypes()
            return (m in mts) or any(m.endswith(x) for x in ("/mp4", "/mpeg", "/wav"))
        except Exception:
            return True
