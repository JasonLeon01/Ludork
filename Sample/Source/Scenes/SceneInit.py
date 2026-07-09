# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
import os
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Callable, Optional, Tuple
import Engine
from Engine import Pair, Color, Vector2f, RenderTexture, RectangleShape
from Engine.Utils import Math
from Engine.Animation import compressAnimation
from Engine.Utils import File
from Engine.UI.Base import SpriteBase, FunctionalBase
from Engine.UI import Image
from Global import Manager, System, SceneBase
from Source.System import System as GameSystem
from .. import Data
from .SceneTitle import Scene as SceneTitle


_MAX_INIT_WORKERS = 4


def _splitCompound(fileName: str) -> Tuple[str, str]:
    parts = fileName.split(".", 1)
    if len(parts) == 2:
        return parts[0], f".{parts[1]}"
    return fileName, ""


def _getCompressedAnimationPath(file: str, animationRoot: str) -> str:
    namePart, _ = _splitCompound(file)
    return os.path.join(animationRoot, f"{namePart}.anim.dat")


def _needsAnimationCompression(file: str, animationRoot: str) -> bool:
    sourcePath = os.path.join(animationRoot, file)
    compressedPath = _getCompressedAnimationPath(file, animationRoot)
    if not os.path.exists(compressedPath):
        return True
    return os.path.getmtime(compressedPath) < os.path.getmtime(sourcePath)


def _compressAnimationFile(file: str, animationRoot: str, assetsRoot: str) -> None:
    _, extensionPart = _splitCompound(file)
    sourcePath = os.path.join(animationRoot, file)
    compressedPath = _getCompressedAnimationPath(file, animationRoot)
    logging.info("Compressing animation: %s", file)
    if extensionPart == ".json":
        payload = File.getJSONData(sourcePath)
    else:
        payload = File.loadData(sourcePath)
    compressed = compressAnimation(payload, assetsRoot=assetsRoot)
    File.saveData(compressedPath, compressed)


class ProgressBar(SpriteBase, FunctionalBase):
    r"""\brief Progress bar for the initial loading scene."""

    def __init__(self, rect: Tuple[Pair[int], Pair[int]]) -> None:
        r"""\brief Construct a progress bar.

        - \param rect A tuple of (position, size) pairs defining the bar's rectangle.
        """
        position, size = rect
        x, y = position
        w, h = size
        self.size = Vector2f(w, h)
        self.realSize = self.size * Engine.Scale
        self.canvas = RenderTexture(Math.ToVector2u(self.realSize))
        self.backRect = RectangleShape(Math.ToVector2f(self.realSize))
        self.backRect.setFillColor(Color(255, 255, 255, 64))
        self.fillRect = RectangleShape(Vector2f(0, self.realSize.y))
        self.fillRect.setFillColor(Color.White)
        self.progressValue = 0.0
        SpriteBase.__init__(self, self.canvas.getTexture())
        FunctionalBase.__init__(self)
        self.setPosition((x, y))

    def setProgress(self, value: float) -> None:
        r"""\brief Set the progress bar's fill value.

        - \param value The progress value (0.0 to 1.0).
        """
        self.progressValue = Math.Clamp(value, 0.0, 1.0)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the progress bar visual.

        - \param deltaTime Elapsed time in seconds.
        """
        fillWidth = self.realSize.x * self.progressValue
        self.fillRect.setSize(Vector2f(fillWidth, self.realSize.y))
        self.canvas.clear(Color.Transparent)
        self.canvas.draw(self.backRect)
        if fillWidth > 0:
            self.canvas.draw(self.fillRect)
        self.canvas.display()


class Scene(SceneBase):
    r"""\brief Initial loading scene that bootstraps game data."""

    def onCreate(self) -> None:
        r"""\brief Create progress bar UI and start asset preparation thread."""
        gameSize = System.getGameSize()
        barWidth = int(gameSize.x * 0.6)
        barHeight = 12
        barX = int((gameSize.x - barWidth) / 2)
        barY = int(gameSize.y * 0.8)
        self._bg = Image(Manager.loadSystem(GameSystem.getTitleBackgroundFile()))
        self._uiManager.loadUI(self._bg)
        self.ProgressBar = ProgressBar(((barX, barY), (barWidth, barHeight)))
        self._uiManager.loadUI(self.ProgressBar)
        self.progressValue = 0.0
        self._displayProgress = 0.0
        self.progressTotal = self._countInitWorkUnits()
        self.processedCount = 0
        self._progressLock = threading.Lock()
        self.progressDone = False
        self.hasSwitched = False
        self.prepareThread = threading.Thread(target=self.prepareAssets, daemon=True)
        self.prepareThread.start()

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the progress bar and transition when done.

        - \param deltaTime Elapsed time in seconds.
        """
        if self.progressTotal > 0:
            with self._progressLock:
                target = 1.0 if self.progressDone else self.progressValue
            self._displayProgress = target
            self.ProgressBar.setProgress(self._displayProgress)
        else:
            self.ProgressBar.setProgress(1.0 if self.progressDone else 0.0)
        if self.progressDone and not self.hasSwitched and self._displayProgress >= 0.999:
            self.hasSwitched = True
            System.setScene(SceneTitle())

    def splitCompound(self, fileName: str) -> Tuple[str, str]:
        r"""\brief Split a compound filename into name and extension.

        - \param fileName The compound filename.

        - \return A tuple of (name, extension).
        """
        return _splitCompound(fileName)

    def _listAnimationSourceFiles(self, animationRoot: str) -> list[str]:
        fileList: list[str] = []
        for file in os.listdir(animationRoot):
            if file.endswith(".anim.dat"):
                continue
            _, extensionPart = _splitCompound(file)
            if extensionPart in [".json", ".dat"]:
                fileList.append(file)
        return fileList

    def _countInitWorkUnits(self) -> int:
        animationRoot = os.path.join(".", "Data", "Animations")
        sourceCount = 0
        loadAnimCount = 0
        if os.path.exists(animationRoot):
            sourceCount = len(self._listAnimationSourceFiles(animationRoot))
            loadAnimCount = Data.countLoadableFiles(
                animationRoot, ".anim.dat", {".anim.dat": File.loadData}
            )
        loadAnimCount = max(sourceCount, loadAnimCount)
        total = sourceCount + loadAnimCount
        total += Data.countLoadableFiles(
            os.path.join(".", "Data", "CommonFunctions"), ".dat", {".dat": File.loadData}
        )
        total += Data.countLoadableFiles(os.path.join(".", "Data", "Tilesets"))
        total += Data.countLoadableFiles(os.path.join(".", "Data", "AutoTiles"))
        total += Data.countLoadableFiles(os.path.join(".", "Data", "General"))
        total += Data.countLoadableFiles(os.path.join(".", "Data", "Curves"), recursive=True)
        return total

    def _advanceProgress(self) -> None:
        with self._progressLock:
            self.processedCount += 1
            if self.progressTotal > 0:
                self.progressValue = self.processedCount / self.progressTotal

    def compressAnimations(self) -> None:
        r"""\brief Compress animation data files if source is newer than cached copies."""
        animationRoot = os.path.join(".", "Data", "Animations")
        if not os.path.exists(animationRoot):
            raise FileNotFoundError(f"Error: Animation data path {animationRoot} does not exist.")
        assetsRoot = os.path.join(".", "Assets", "Animations")
        sourceFiles = self._listAnimationSourceFiles(animationRoot)
        if not sourceFiles:
            return
        fileList = [file for file in sourceFiles if _needsAnimationCompression(file, animationRoot)]
        for _ in range(len(sourceFiles) - len(fileList)):
            self._advanceProgress()
        if not fileList:
            return
        maxWorkers = min(len(fileList), os.cpu_count() or _MAX_INIT_WORKERS, _MAX_INIT_WORKERS)
        with ThreadPoolExecutor(max_workers=maxWorkers) as executor:
            futures = [
                executor.submit(_compressAnimationFile, file, animationRoot, assetsRoot) for file in fileList
            ]
            for future in as_completed(futures):
                future.result()
                self._advanceProgress()

    def loadGameData(self) -> None:
        r"""\brief Load all independent game data phases and update the progress bar."""
        onProgress = self._advanceProgress
        phases = [
            ("animations", Data.loadAnimations),
            ("common functions", Data.loadCommonFunctions),
            ("tilesets", Data.loadTilesets),
            ("autotiles", Data.loadAutoTiles),
            ("general data", Data.loadGeneralData),
            ("curves", Data.loadCurves),
        ]
        maxWorkers = min(len(phases), os.cpu_count() or _MAX_INIT_WORKERS, _MAX_INIT_WORKERS)
        with ThreadPoolExecutor(max_workers=maxWorkers) as executor:
            futures = {
                executor.submit(self._loadGameDataPhase, phaseName, loadFn, onProgress): phaseName
                for phaseName, loadFn in phases
            }
            for future in as_completed(futures):
                future.result()

    def _loadGameDataPhase(
        self, phaseName: str, loadFn: Callable[[Optional[Callable[[], None]]], None], onProgress: Callable[[], None]
    ) -> None:
        startTime = time.perf_counter()
        logging.info("Loading %s", phaseName)
        loadFn(onProgress)
        logging.info("Loaded %s in %.3fs", phaseName, time.perf_counter() - startTime)

    def prepareAssets(self) -> None:
        r"""\brief Background thread entry point: compress animations then load all data."""
        startTime = time.perf_counter()
        compressStartTime = time.perf_counter()
        self.compressAnimations()
        logging.info("Prepared animation cache files in %.3fs", time.perf_counter() - compressStartTime)
        loadStartTime = time.perf_counter()
        self.loadGameData()
        logging.info("Loaded game data in %.3fs", time.perf_counter() - loadStartTime)
        with self._progressLock:
            self.progressValue = 1.0
        self.progressDone = True
        logging.info("Init asset preparation finished in %.3fs", time.perf_counter() - startTime)
