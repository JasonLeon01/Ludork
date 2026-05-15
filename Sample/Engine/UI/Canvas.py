# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import List, Tuple, Union, TYPE_CHECKING
from .. import TypeAdapter, Pair, IntRect, Vector2i, Vector2f, Vector2u, RenderTexture, Color, View
from ..Animation import AnimSprite
from ..Utils import Math, Render
from .Base import SpriteBase, FunctionalBase

if TYPE_CHECKING:
    from Engine.UI.Base import ControlBase


class Canvas(SpriteBase, FunctionalBase):
    r"""Drawable canvas widget that manages an off-screen render target and UI child tree.

    Provides a RenderTexture-backed canvas that supports hierarchical UI rendering,
    z-ordering, view manipulation, and child control management.
    """

    @TypeAdapter(rect=([tuple, list], IntRect, lambda pos, size: IntRect(Vector2i(*pos), Vector2i(*size))))
    def __init__(self, rect: Union[IntRect, Tuple[Pair[int], Pair[int]], List[List[int]]]) -> None:
        r"""\brief Construct a Canvas with a given logical rectangle.

        - \param rect  Logical position and size of the canvas.
                      Can also accept a pair of pairs or a 2x2 list.
        """

        from .. import Scale

        self._inRect = rect
        self._size = Math.ToVector2u(rect.size)
        size = Math.ToVector2u(Math.ToVector2f(rect.size) * Scale)
        self._canvas: RenderTexture = RenderTexture(size)
        self._childrenList: List[ControlBase] = []
        self._renderQueue: List[tuple[ControlBase, object]] = []
        self._anims: List[AnimSprite] = []
        self._zOrder: int = 0
        SpriteBase.__init__(self, self._canvas.getTexture())
        FunctionalBase.__init__(self)
        self.setPosition(Math.ToVector2f(rect.position))

    def v_getOrigin(self) -> Pair[float]:
        r"""\brief Get the canvas origin as a plain pair.

        - \return  Origin as (x, y) in logical UI units
        """
        origin = self.getOrigin()
        return (origin.x, origin.y)

    def getOrigin(self) -> Vector2f:
        r"""\brief Get the canvas origin in logical UI units.

        - \return  Origin position in logical UI units
        """
        from .. import Scale

        origin = super().getOrigin()
        return origin / Scale

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the canvas origin in logical UI units.

        - \param origin  Origin position in logical UI units
        """
        from .. import Scale

        super().setOrigin(origin * Scale)

    def getSize(self) -> Vector2u:
        r"""\brief Get the canvas size in logical UI units.

        - \return  Canvas size in logical UI units
        """
        return self._size

    def getNoTranslationRect(self) -> IntRect:
        r"""\brief Get the local rectangle ignoring any translation.

        - \return  Local rectangle starting at (0, 0)
        """
        return IntRect(Vector2i(0, 0), Math.ToVector2i(self._size))

    def getContentRect(self) -> IntRect:
        r"""\brief Get the content area rectangle with padding.

        - \return  Content rectangle with 16px padding on each side
        """
        return IntRect(Vector2i(16, 16), Vector2i(self._size.x - 32, self._size.y - 32))

    def getView(self) -> View:
        r"""\brief Get the current view of the canvas in logical UI units.

        - \return  Current view in logical UI units
        """
        from .. import Scale

        view = self._canvas.getView()
        return View(view.getCenter() / Scale, view.getSize() / Scale)

    def getDefaultView(self) -> View:
        r"""\brief Get the default view of the canvas in logical UI units.

        - \return  Default view in logical UI units
        """
        from .. import Scale

        view = self._canvas.getDefaultView()
        return View(view.getCenter() / Scale, view.getSize() / Scale)

    def setView(self, view: View) -> None:
        r"""\brief Set the current view of the canvas.

        - \param view  New view in logical UI units
        """
        from .. import Scale

        newView = View(view.getCenter() * Scale, view.getSize() * Scale)
        self._canvas.setView(newView)

    def getChildren(self) -> List[ControlBase]:
        r"""\brief Get the list of child controls attached to this canvas.

        - \return  List of child controls
        """
        return self._childrenList

    def addChild(self, child: ControlBase) -> None:
        r"""\brief Add a child control to this canvas.

        - \param child  Control to add (must not be an Actor)
        """
        from ..Gameplay.Actors import Actor

        assert not isinstance(child, Actor), "Cannot add Actor to UI"
        self._childrenList.append(child)
        child.setParent(self)

    def removeChild(self, child: ControlBase) -> None:
        r"""\brief Remove a child control from this canvas.

        - \param child  Control to remove
        - \throws ValueError  If the child is not found
        """
        if child not in self._childrenList:
            raise ValueError("Child not found")
        self._childrenList.remove(child)
        child.setParent(None)

    def addAnim(self, anim: AnimSprite) -> None:
        r"""\brief Add an animation sprite to this canvas.

        - \param anim  The animation sprite to add
        """
        self._anims.append(anim)

    def removeAnim(self, anim: AnimSprite) -> None:
        r"""\brief Remove an animation sprite from this canvas.

        - \param anim  The animation sprite to remove
        - \throws ValueError  If the animation is not found
        """
        if anim not in self._anims:
            raise ValueError("Animation not found")
        self._anims.remove(anim)

    def clearAnims(self) -> None:
        r"""\brief Remove all animation sprites from this canvas."""
        self._anims.clear()

    def getAnims(self) -> List[AnimSprite]:
        r"""\brief Get the list of animation sprites on this canvas.

        - \return  List of AnimSprite objects
        """
        return self._anims

    def setZOrder(self, zOrder: int) -> None:
        r"""\brief Set the z-order of this canvas.

        - \param zOrder  New z-order value
        """
        self._zOrder = zOrder

    def getZOrder(self) -> int:
        r"""\brief Get the z-order of this canvas.

        - \return  Current z-order value
        """
        return self._zOrder

    def update(self, deltaTime: float) -> None:
        r"""\brief Update this canvas and its active, visible children.

        - \param deltaTime  Time elapsed since last update, in seconds
        """
        for child in self._childrenList:
            if child.getActive() and child.getVisible():
                if hasattr(child, "update"):
                    child.update(deltaTime)
        for anim in self._anims[:]:
            if anim.isFinished():
                self._anims.remove(anim)
        for anim in self._anims:
            anim.update(deltaTime)
        FunctionalBase.update(self, deltaTime)
        self._buildRenderQueue()

    def render(self) -> None:
        r"""\brief Render the canvas to its internal RenderTexture.

        Clears the canvas, draws all animations and queued nodes, and displays the result.
        """
        self._canvas.clear(Color.Transparent)
        for anim in self._anims:
            self._canvas.draw(anim, Render.CanvasRenderStates())
        for node, nodeStates in self._renderQueue:
            self._canvas.draw(node, copy.copy(nodeStates))
        self._canvas.display()

    def lateUpdate(self, deltaTime: float) -> None:
        r"""\brief Run late update on this canvas and its children.

        - \param deltaTime  Time elapsed since last update, in seconds
        """
        for child in self._childrenList:
            if child.getActive() and child.getVisible() and hasattr(child, "lateUpdate"):
                child.lateUpdate(deltaTime)
        FunctionalBase.lateUpdate(self, deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        r"""\brief Run fixed-timestep update on this canvas and its children.

        - \param fixedDelta  Fixed timestep duration, in seconds
        """
        for child in self._childrenList:
            if child.getActive() and child.getVisible() and hasattr(child, "fixedUpdate"):
                child.fixedUpdate(fixedDelta)
        FunctionalBase.fixedUpdate(self, fixedDelta)

    def _appendRenderNode(self, node: ControlBase, parentStates) -> None:
        from .ListView import ListView

        if hasattr(node, "applyPositions"):
            node.applyPositions()
        nodeStates = copy.copy(parentStates)
        if hasattr(node, "getRenderStates"):
            nodeStates = copy.copy(node.getRenderStates())
            nodeStates.transform *= parentStates.transform
        if not isinstance(node, ListView):
            self._renderQueue.append((node, nodeStates))
        childStates = copy.copy(parentStates)
        childStates.transform *= node._getRenderTransform()
        if hasattr(node, "getChildren"):
            for child in node.getChildren():
                if child.getVisible():
                    self._appendRenderNode(child, childStates)

    def _buildRenderQueue(self) -> None:
        self._renderQueue.clear()
        baseStates = Render.CanvasRenderStates()
        for child in self._childrenList:
            if child.getVisible():
                self._appendRenderNode(child, baseStates)
