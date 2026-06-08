# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict, Optional

from Engine import Filters, Music, Pair
from Global import Manager


class SceneMapAudioController:
    r"""\brief Manage map BGM/BGS playback and music filters."""

    def __init__(self) -> None:
        self._currentBgmMusic: Optional[Music] = None
        self._currentBgmFile: str = ""
        self._currentBgsMusic: Optional[Music] = None
        self._currentBgsFile: str = ""

    def setBgmFilter(self, attr: str, value: Any) -> None:
        r"""\brief Set a filter attribute on the current BGM music.

        - \param attr The filter attribute name.
        - \param value The filter attribute value.
        """
        if self._currentBgmMusic is None:
            return
        data: Dict[str, Any] = {attr: value}
        if attr == "loopPoint" and isinstance(value, (Pair, tuple, list)):
            data["loopPoint"] = {"start": value[0], "end": value[1]}
        filterObj = self._buildMusicFilter(data)
        if filterObj is not None:
            from Global.Manager.Mgr_Audio import AudioManager

            AudioManager.setMusicFilter(self._currentBgmMusic, filterObj)

    def setBgsFilter(self, attr: str, value: Any) -> None:
        r"""\brief Set a filter attribute on the current BGS music.

        - \param attr The filter attribute name.
        - \param value The filter attribute value.
        """
        if self._currentBgsMusic is None:
            return
        data: Dict[str, Any] = {attr: value}
        if attr == "loopPoint" and isinstance(value, (Pair, tuple, list)):
            data["loopPoint"] = {"start": value[0], "end": value[1]}
        filterObj = self._buildMusicFilter(data)
        if filterObj is not None:
            from Global.Manager.Mgr_Audio import AudioManager

            AudioManager.setMusicFilter(self._currentBgsMusic, filterObj)

    def playMapAudio(self, mapData: Dict[str, Any]) -> None:
        r"""\brief Play or reuse BGM/BGS described by map data.

        - \param mapData Current map data.
        """
        bgm = mapData.get("bgm", "")
        bgmFilter = self._buildMusicFilter(mapData.get("bgmFilter", {}))
        reuseBgm = bool(bgm) and self._currentBgmMusic is not None and self._currentBgmFile == bgm
        if reuseBgm:
            if bgmFilter is not None:
                from Global.Manager.Mgr_Audio import AudioManager

                AudioManager.setMusicFilter(self._currentBgmMusic, bgmFilter)
            self._currentBgmMusic.setLooping(True)
        else:
            if self._currentBgmMusic is not None:
                Manager.stopMusic("BGM")
                self._currentBgmMusic = None
            self._currentBgmFile = ""
        if bgm:
            if not reuseBgm:
                self._currentBgmMusic = Manager.playMusic("BGM", bgm, bgmFilter)
                if self._currentBgmMusic is not None:
                    self._currentBgmMusic.setLooping(True)
                    self._currentBgmFile = bgm
        bgs = mapData.get("bgs", "")
        bgsFilter = self._buildMusicFilter(mapData.get("bgsFilter", {}))
        reuseBgs = bool(bgs) and self._currentBgsMusic is not None and self._currentBgsFile == bgs
        if reuseBgs:
            if bgsFilter is not None:
                from Global.Manager.Mgr_Audio import AudioManager

                AudioManager.setMusicFilter(self._currentBgsMusic, bgsFilter)
            self._currentBgsMusic.setLooping(True)
        else:
            if self._currentBgsMusic is not None:
                Manager.stopMusic("BGS")
                self._currentBgsMusic = None
            self._currentBgsFile = ""
        if bgs:
            if not reuseBgs:
                self._currentBgsMusic = Manager.playMusic("BGS", bgs, bgsFilter)
                if self._currentBgsMusic is not None:
                    self._currentBgsMusic.setLooping(True)
                    self._currentBgsFile = bgs

    def stopMapAudio(self) -> None:
        r"""\brief Stop current BGM/BGS playback."""
        if self._currentBgmMusic is not None:
            Manager.stopMusic("BGM")
            self._currentBgmMusic = None
        self._currentBgmFile = ""
        if self._currentBgsMusic is not None:
            Manager.stopMusic("BGS")
            self._currentBgsMusic = None
        self._currentBgsFile = ""

    def _buildMusicFilter(self, data: Dict[str, Any]) -> Optional[Filters.MusicFilter]:
        if not data:
            return None
        kwargs: Dict[str, Any] = {}
        for key in ("loop", "offset", "pitch", "pan", "volume"):
            if key in data:
                kwargs[key] = data[key]
        if "loopPoint" in data and isinstance(data["loopPoint"], dict):
            lp = data["loopPoint"]
            kwargs["loopPoint"] = (float(lp.get("start", 0.0)), float(lp.get("end", 0.0)))
            if "offset" not in kwargs:
                start = float(lp.get("start", 0.0))
                if start > 0.0:
                    kwargs["offset"] = start
        if not kwargs:
            return None
        return Filters.MusicFilter(**kwargs)
