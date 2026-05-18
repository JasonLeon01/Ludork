# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple, Callable, List
from Engine import Pair, Image, IntRect, Vector2i, Vector2f, Texture, UI
from Engine.UI import Canvas, Window, Image as UIImage
from Engine.Utils import Math
from Global import Manager


class WindowBase(Canvas):
    r"""\brief Base class for all game windows.

    Provides a window skin, content area, and nested canvas hierarchy.
    """

    _PAUSE_MARK_SIZE = 16
    _PAUSE_MARK_Y_OFFSET = 4
    _PAUSE_MARK_FRAME_INTERVAL = 0.125
    _PAUSE_MARK_ATLAS_RECT = Math.ToIntRect(160, 64, 32, 32)
    _PAUSE_MARK_FRAME_RECTS: List[IntRect] = [
        Math.ToIntRect(0, 0, 16, 16),
        Math.ToIntRect(16, 0, 16, 16),
        Math.ToIntRect(0, 16, 16, 16),
        Math.ToIntRect(16, 16, 16, 16),
    ]

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        r"""\brief Construct a window with a skin and content area.

        - \param rect The window rectangle.
        - \param windowSkin Optional window skin image; defaults to the system windowskin.
        - \param repeated Whether the window skin is repeated.
        """
        super().__init__(rect)
        if windowSkin is None:
            windowSkin = Manager.loadSystem(UI.DefaultWindowskinName, smooth=True).copyToImage()
        self._windowSkin = windowSkin
        self._window = Window(self.getNoTranslationRect(), windowSkin, repeated)
        self.content = Canvas(self.getContentRect())
        self.addChild(self._window)
        self.addChild(self.content)
        self._pauseMarkShowRequested: bool = False
        self._pauseMarkEnabled: bool = True
        self._pauseMarkVisiblePredicate: Optional[Callable[[], bool]] = None
        self._pauseMarkFrameIndex: int = 0
        self._pauseMarkFrameTimer: float = 0.0
        self._pauseMarkTexture = Texture(self._windowSkin, False, self._PAUSE_MARK_ATLAS_RECT)
        self._pauseMarkTexture.setSmooth(False)
        self._pauseMark = UIImage(self._pauseMarkTexture)
        self._pauseMark.setTextureRect(self._PAUSE_MARK_FRAME_RECTS[0])
        self._pauseMark.setVisible(False)
        self.content.addChild(self._pauseMark)

    def setPauseMarkEnabled(self, enabled: bool) -> None:
        r"""\brief Enable or disable the pause mark display.

        - \param enabled Whether the pause mark is allowed to show.
        """
        self._pauseMarkEnabled = enabled
        self._refreshPauseMarkVisibility()

    def setPauseMarkVisiblePredicate(self, predicate: Optional[Callable[[], bool]]) -> None:
        r"""\brief Set an optional predicate that gates pause mark visibility.

        - \param predicate Callable returning True when the pause mark may show, or None to clear.
        """
        self._pauseMarkVisiblePredicate = predicate
        self._refreshPauseMarkVisibility()

    def showPauseMark(self) -> None:
        r"""\brief Request the pause mark to be shown (subject to enabled state and predicate)."""
        self._pauseMarkShowRequested = True
        self._refreshPauseMarkVisibility()

    def hidePauseMark(self) -> None:
        r"""\brief Hide the pause mark."""
        self._pauseMarkShowRequested = False
        self._refreshPauseMarkVisibility()

    def refreshPauseMarkLayout(self) -> None:
        r"""\brief Position the pause mark at the bottom-centre of the content area."""
        contentSize = self.content.getSize()
        posX = (float(contentSize.x) - float(self._PAUSE_MARK_SIZE)) / 2.0
        posY = float(contentSize.y) - float(self._PAUSE_MARK_SIZE) + float(self._PAUSE_MARK_Y_OFFSET)
        self._pauseMark.setPosition(Vector2f(posX, posY))
        self._bringPauseMarkToFront()

    def _bringPauseMarkToFront(self) -> None:
        if self._pauseMark.getParent() is self.content:
            self.content.removeChild(self._pauseMark)
            self.content.addChild(self._pauseMark)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update pause mark animation.

        - \param deltaTime Elapsed time in seconds.
        """
        super().onTick(deltaTime)
        self._updatePauseMarkAnimation(deltaTime)

    def _refreshPauseMarkVisibility(self) -> None:
        visible = self._pauseMarkShowRequested and self._pauseMarkEnabled
        if visible and self._pauseMarkVisiblePredicate is not None:
            visible = self._pauseMarkVisiblePredicate()
        self._pauseMark.setVisible(visible)
        if not visible:
            self._pauseMarkFrameIndex = 0
            self._pauseMarkFrameTimer = 0.0
            self._pauseMark.setTextureRect(self._PAUSE_MARK_FRAME_RECTS[0])

    def _updatePauseMarkAnimation(self, deltaTime: float) -> None:
        if not self._pauseMark.getVisible():
            return
        if self._pauseMarkVisiblePredicate is not None:
            self._refreshPauseMarkVisibility()
            if not self._pauseMark.getVisible():
                return
        self._pauseMarkFrameTimer += deltaTime
        if self._pauseMarkFrameTimer < self._PAUSE_MARK_FRAME_INTERVAL:
            return
        self._pauseMarkFrameTimer -= self._PAUSE_MARK_FRAME_INTERVAL
        self._pauseMarkFrameIndex = (self._pauseMarkFrameIndex + 1) % len(self._PAUSE_MARK_FRAME_RECTS)
        self._pauseMark.setTextureRect(self._PAUSE_MARK_FRAME_RECTS[self._pauseMarkFrameIndex])
