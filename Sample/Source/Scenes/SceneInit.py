# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import threading
from typing import Tuple
import Engine
from Engine import Pair, Color, Vector2f, RenderTexture, RectangleShape
from Engine.Utils import Math
from Engine.Animation import compressAnimation
from Engine.Utils import File
from Engine.UI.Base import SpriteBase, FunctionalBase
from Engine.UI import Image
from Global import Manager, System, SceneBase
from .. import Data
from .SceneTitle import Scene as SceneTitle


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

    def update(self, deltaTime: float) -> None:
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
        self._bg = Image(Manager.loadSystem("GrassBackground.png"))
        self._uiManager.loadUI(self._bg)
        self.ProgressBar = ProgressBar(((barX, barY), (barWidth, barHeight)))
        self._uiManager.loadUI(self.ProgressBar)
        self.progressValue = 0.0
        self.progressTotal = Data.getDataKinds()
        self.processedCount = 0
        self.progressDone = False
        self.hasSwitched = False
        self.prepareThread = threading.Thread(target=self.prepareAssets, daemon=True)
        self.prepareThread.start()

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the progress bar and transition when done.

        - \param deltaTime Elapsed time in seconds.
        """
        if self.progressTotal > 0:
            self.ProgressBar.setProgress(self.progressValue)
        else:
            self.ProgressBar.setProgress(1.0 if self.progressDone else 0.0)
        if self.progressDone and not self.hasSwitched:
            self.hasSwitched = True
            System.setScene(SceneTitle())

    def splitCompound(self, fileName: str) -> Tuple[str, str]:
        r"""\brief Split a compound filename into name and extension.

        - \param fileName The compound filename.

        - \return A tuple of (name, extension).
        """
        parts = fileName.split(".", 1)
        if len(parts) == 2:
            return parts[0], f".{parts[1]}"
        return fileName, ""

    def compressAnimations(self) -> None:
        r"""\brief Compress animation data files if source is newer than cached copies."""
        animationRoot = os.path.join(".", "Data", "Animations")
        if not os.path.exists(animationRoot):
            raise FileNotFoundError(f"Error: Animation data path {animationRoot} does not exist.")
        assetsRoot = os.path.join(".", "Assets", "Animations")
        fileList = []
        for file in os.listdir(animationRoot):
            if file.endswith(".anim.dat"):
                continue
            namePart, extensionPart = self.splitCompound(file)
            if extensionPart not in [".json", ".dat"]:
                continue
            fileList.append(file)
        self.progressTotal += len(fileList)
        for file in fileList:
            namePart, extensionPart = self.splitCompound(file)
            sourcePath = os.path.join(animationRoot, file)
            compressedPath = os.path.join(animationRoot, f"{namePart}.anim.dat")
            needCompress = True
            if os.path.exists(compressedPath):
                if os.path.getmtime(compressedPath) >= os.path.getmtime(sourcePath):
                    needCompress = False
            if needCompress:
                if extensionPart == ".json":
                    payload = File.getJSONData(sourcePath)
                else:
                    payload = File.loadData(sourcePath)
                compressed = compressAnimation(payload, assetsRoot=assetsRoot)
                File.saveData(compressedPath, compressed)
            self.processedCount += 1
            if self.progressTotal > 0:
                self.progressValue = self.processedCount / self.progressTotal

    def loadGameData(self) -> None:
        r"""\brief Load all game data in sequence and update the progress bar."""
        Data.loadAnimations()
        self.processedCount += 1
        if self.progressTotal > 0:
            self.progressValue = self.processedCount / self.progressTotal
        Data.loadCommonFunctions()
        self.processedCount += 1
        if self.progressTotal > 0:
            self.progressValue = self.processedCount / self.progressTotal
        Data.loadTilesets()
        self.processedCount += 1
        Data.loadAutoTiles()
        self.processedCount += 1
        Data.loadGeneralData()
        self.processedCount += 1
        if self.progressTotal > 0:
            self.progressValue = self.processedCount / self.progressTotal

    def prepareAssets(self) -> None:
        r"""\brief Background thread entry point: compress animations then load all data."""
        self.compressAnimations()
        self.loadGameData()
        self.progressValue = 1.0
        self.progressDone = True
