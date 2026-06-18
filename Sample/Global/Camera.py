# -*- encoding: utf-8 -*-
r"""\brief Camera system: viewport tracking, smooth follow, and screen-space transforms."""

from __future__ import annotations
from typing import Optional, Union, List, TYPE_CHECKING
import Engine
from Engine import (
    Pair,
    Vector2u,
    Transformable,
    Texture,
    Image,
    Drawable,
    RenderTexture,
    Sprite,
    FloatRect,
    Vector2i,
    Vector2f,
    View,
    RenderTarget,
    RenderStates,
    Color,
    Angle,
    degrees,
)
from Engine.Utils import Math, Render
from .System import System

if TYPE_CHECKING:
    from Global import GameMap
    from Engine.Gameplay.Actors import Actor


class Camera(Drawable, Transformable):
    r"""\brief Camera with viewport tracking, smooth follow, and off-screen rendering.

    Manages a render texture for off-screen drawing, supports viewport
    transforms, and can follow a parent actor with position clamping.
    """

    def __init__(self, viewport: Optional[FloatRect] = None) -> None:
        r"""\brief Construct a camera with an optional viewport.

        - \param viewport Initial viewport rectangle; defaults to the game size.
        """
        Drawable.__init__(self)
        Transformable.__init__(self)
        self._viewport = viewport
        if self._viewport is None:
            self._viewport = FloatRect(Vector2f(0, 0), Math.ToVector2f(System.getGameSize()))
        self._renderTexture: RenderTexture
        assert isinstance(self._viewport, FloatRect)
        size = Math.ToVector2u(self._viewport.size)
        self._renderTexture = RenderTexture(size)
        self._renderTexture.setView(View(self._viewport))
        self._renderSprite = Sprite(self._renderTexture.getTexture())
        self._canvases: List[RenderTexture] = []
        self._renderStates = Render.CanvasRenderStates()
        self._parent: Optional[Actor] = None
        self._map: Optional[GameMap] = None

    @ExecSplit(default=(None,))
    def setViewport(self, inViewport: FloatRect) -> None:
        r"""\brief Set the camera viewport rectangle.

        - \param inViewport The new viewport rectangle.
        """
        assert isinstance(inViewport, FloatRect)
        self._viewport = inViewport
        self._refreshView()

    @ReturnType(view=View)
    def getView(self) -> View:
        r"""\brief Get the current render view.

        - \return The current View object.
        """
        return self._renderTexture.getView()

    @ReturnType(position=Vector2f)
    def getViewPosition(self) -> Optional[Vector2f]:
        r"""\brief Get the current viewport position.

        - \return The viewport position, or None if no viewport is set.
        """
        return self._viewport and self._viewport.position or None

    @ReturnType(position=Pair[float])
    def v_getViewPosition(self) -> Optional[Pair[float]]:
        r"""\brief Get the viewport position as a tuple.

        - \return The viewport position as (x, y), or None.
        """
        return self._viewport and (self._viewport.position.x, self._viewport.position.y) or None

    @Meta(Vector2fVars=["inPosition"])
    @ExecSplit(default=(None,))
    def setViewPosition(self, inPosition: Vector2f) -> None:
        r"""\brief Set the viewport position.

        - \param inPosition The new viewport position.
        """
        if self._viewport:
            self._viewport.position = inPosition
            self._refreshView()

    @ReturnType(size=Vector2f)
    def getViewSize(self) -> Optional[Vector2f]:
        r"""\brief Get the current viewport size.

        - \return The viewport size, or None if no viewport is set.
        """
        return self._viewport and self._viewport.size or None

    @ReturnType(size=Pair[float])
    def v_getViewSize(self) -> Optional[Pair[float]]:
        r"""\brief Get the viewport size as a tuple.

        - \return The viewport size as (w, h), or None.
        """
        return self._viewport and (self._viewport.size.x, self._viewport.size.y) or None

    @Meta(Vector2fVars=["inSize"])
    @ExecSplit(default=(None,))
    def setViewSize(self, inSize: Vector2f) -> None:
        r"""\brief Set the viewport size.

        - \param inSize The new viewport size.
        """
        if self._viewport:
            self._viewport.size = inSize
            self._refreshView()

    @ReturnType(rotation=Angle)
    def getViewRotation(self) -> Angle:
        r"""\brief Get the view rotation as an Angle.

        - \return The view rotation.
        """
        return self._renderTexture.getView().getRotation()

    @ReturnType(rotation=float)
    def v_getViewRotation(self) -> float:
        r"""\brief Get the view rotation in degrees.

        - \return The view rotation in degrees.
        """
        return self._renderTexture.getView().getRotation().asDegrees()

    @ExecSplit(default=(None,))
    @TypeAdapter(inRotation=(float, Angle, degrees))
    def setViewRotation(self, inRotation: Union[Angle, float]) -> None:
        r"""\brief Set the view rotation.

        - \param inRotation The new rotation (Angle or float in degrees).
        """
        view = self._renderTexture.getView()
        view.setRotation(inRotation)
        self._renderTexture.setView(view)
        for canvas in self._canvases:
            canvas.setView(view)

    @Meta(Vector2fVars=["delta"])
    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(tuple, Vector2f))
    def moveView(self, delta: Union[Vector2f, Pair[float]]) -> None:
        r"""\brief Move the viewport by a delta offset.

        - \param delta The offset to move the viewport by.
        """
        if self._viewport:
            self.setViewPosition(self._viewport.position + delta)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(float, Angle, degrees))
    def rotateView(self, delta: Union[Angle, float]) -> None:
        r"""\brief Rotate the view by a delta.

        - \param delta The rotation delta (Angle or float in degrees).
        """
        self.setViewRotation(self.getViewRotation() + delta)

    @ExecSplit(default=(None,))
    def resumeViewport(self) -> None:
        r"""\brief Reset the view to the render texture default view."""
        self._renderTexture.setView(self._renderTexture.getDefaultView())

    @ReturnType(position=Pair[float])
    def v_getPosition(self) -> Pair[float]:
        r"""\brief Get the camera transform position as a tuple.

        - \return The transform position as (x, y).
        """
        pos = self.getPosition()
        return (pos.x, pos.y)

    @ReturnType(position=Vector2f)
    def getPosition(self) -> Vector2f:
        r"""\brief Get the camera transform position.

        - \return The transform position.
        """
        return super().getPosition()

    @Meta(Vector2fVars=["inPosition"])
    @ExecSplit(default=(None,))
    @TypeAdapter(inPosition=(tuple, Vector2f))
    def setPosition(self, inPosition: Union[Vector2f, Pair[float]]) -> None:
        r"""\brief Set the camera transform position.

        - \param inPosition The new transform position.
        """
        super().setPosition(inPosition)

    @Meta(Vector2fVars=["delta"])
    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(tuple, Vector2f))
    def move(self, delta: Union[Vector2f, Pair[float]]) -> None:
        r"""\brief Move the camera transform by a delta.

        - \param delta The offset to move by.
        """
        super().move(delta)

    @ReturnType(rotation=float)
    def v_getRotation(self) -> float:
        r"""\brief Get the camera transform rotation in degrees.

        - \return The transform rotation in degrees.
        """
        return self.getRotation().asDegrees()

    @ReturnType(rotation=Angle)
    def getRotation(self) -> Angle:
        r"""\brief Get the camera transform rotation.

        - \return The transform rotation as an Angle.
        """
        return super().getRotation()

    @ExecSplit(default=(None,))
    @TypeAdapter(inRotation=(float, Angle, degrees))
    def setRotation(self, inRotation: Union[Angle, float]) -> None:
        r"""\brief Set the camera transform rotation.

        - \param inRotation The new rotation (Angle or float in degrees).
        """
        super().setRotation(inRotation)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(float, Angle, degrees))
    def rotate(self, delta: Union[Angle, float]) -> None:
        r"""\brief Rotate the camera transform by a delta.

        - \param delta The rotation delta (Angle or float in degrees).
        """
        super().rotate(delta)

    @ReturnType(scale=Pair[float])
    def v_getScale(self) -> Pair[float]:
        r"""\brief Get the camera transform scale as a tuple.

        - \return The transform scale as (x, y).
        """
        scale = self.getScale()
        return (scale.x, scale.y)

    @ReturnType(scale=Vector2f)
    def getScale(self) -> Vector2f:
        r"""\brief Get the camera transform scale.

        - \return The transform scale.
        """
        return super().getScale()

    @Meta(Vector2fVars=["factors"])
    @ExecSplit(default=(None,))
    @TypeAdapter(factors=(tuple, Vector2f))
    def setScale(self, factors: Union[Vector2f, Pair[float]]) -> None:
        r"""\brief Set the camera transform scale.

        - \param factors The new scale factors.
        """
        super().setScale(factors)

    @Meta(Vector2fVars=["delta"])
    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(tuple, Vector2f))
    def scale(self, delta: Union[Vector2f, Pair[float]]) -> None:
        r"""\brief Scale the camera transform by a delta.

        - \param delta The scale delta.
        """
        super().scale(delta)

    @ReturnType(parent="Actor")
    def getParent(self) -> Optional[Actor]:
        r"""\brief Get the parent actor this camera follows.

        - \return The parent actor, or None.
        """
        return self._parent

    @ExecSplit(default=(None,))
    def setParent(self, actor: Optional[Actor]) -> None:
        r"""\brief Set the parent actor for the camera to follow.

        - \param actor The actor to follow, or None to stop following.
        """
        self._parent = actor

    @ExecSplit(default=(None,))
    def setMap(self, map: GameMap) -> None:
        r"""\brief Set the game map this camera operates on.

        - \param map The GameMap instance.
        """
        self._map = map

    @ReturnType(map="GameMap")
    def getMap(self) -> Optional[GameMap]:
        r"""\brief Get the game map this camera operates on.

        - \return The GameMap instance, or None.
        """
        return self._map

    def render(self, obj: Drawable) -> None:
        r"""\brief Render a drawable object to the camera's render texture.

        - \param obj The drawable object to render.
        """
        self._renderTexture.draw(obj, self._renderStates)

    def getViewport(self) -> Optional[FloatRect]:
        r"""\brief Get the current viewport rectangle.

        - \return The viewport rectangle, or None.
        """
        return self._viewport

    def getRenderStates(self) -> RenderStates:
        r"""\brief Get the current render states.

        - \return The current RenderStates.
        """
        return self._renderStates

    def setRenderStates(self, inRenderStates: RenderStates) -> None:
        r"""\brief Set the render states.

        - \param inRenderStates The new RenderStates.
        """
        self._renderStates = inRenderStates

    @Meta(Vector2iVars=["point"])
    def mapPixelToCoords(self, point: Vector2i) -> Vector2f:
        r"""\brief Convert a pixel position to world coordinates.

        - \param point The pixel position.
        - \return The world coordinate position.
        """
        return self._renderTexture.mapPixelToCoords(point, self._renderTexture.getView())

    @Meta(Vector2fVars=["point"])
    def mapCoordsToPixel(self, point: Vector2f) -> Vector2i:
        r"""\brief Convert world coordinates to a pixel position.

        - \param point The world coordinate position.
        - \return The pixel position.
        """
        return self._renderTexture.mapCoordsToPixel(point, self._renderTexture.getView())

    def getTexture(self) -> Texture:
        r"""\brief Get the render texture.

        - \return The render texture.
        """
        return self._renderTexture.getTexture()

    @ReturnType(renderTexture=RenderTexture)
    def getRenderTexture(self) -> RenderTexture:
        r"""\brief Get the off-screen render target used for map drawing.

        - \return The camera RenderTexture.
        """
        return self._renderTexture

    def getImage(self) -> Image:
        r"""\brief Get the rendered image.

        - \return The rendered image.
        """
        return self._renderTexture.getTexture().copyToImage()

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Called every frame for per-frame logic.

        - \param deltaTime Elapsed time in seconds since the previous frame.
        """
        pass

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Called after onTick for late-update logic.

        - \param deltaTime Elapsed time in seconds since the previous frame.
        """
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        r"""\brief Called at a fixed timestep for physics-like updates.

        Follows the parent actor and clamps the viewport to map bounds.

        - \param fixedDelta Fixed timestep in seconds.
        """
        if self._map is None:
            return
        if self._parent and self._viewport:
            self.setViewPosition(self._parent.getPosition() - self._viewport.size / 2)
            self.fixViewPosition()

    def fixViewPosition(self) -> None:
        r"""\brief Clamp the viewport position to stay within map bounds."""
        if self._map is None or self._viewport is None:
            return
        pos = self.getViewPosition()
        if pos is None:
            return
        mapSize = self._map.getSize()
        maxX = mapSize.x * Engine.CellSize - self._viewport.size.x
        maxY = mapSize.y * Engine.CellSize - self._viewport.size.y
        px = Math.Clamp(pos.x, 0, maxX if maxX > 0 else 0)
        py = Math.Clamp(pos.y, 0, maxY if maxY > 0 else 0)
        self.setViewPosition(Vector2f(px, py))

    def draw(self, target: RenderTarget, states: RenderStates = RenderStates()) -> None:
        r"""\brief Draw the camera's render sprite to a render target.

        - \param target The render target to draw to.
        - \param states The render states to use.
        """
        states.transform *= self.getTransform()
        target.draw(self._renderSprite, states)

    def clear(self) -> None:
        r"""\brief Clear the camera's render texture to transparent."""
        self._renderTexture.clear(Color.Transparent)

    def display(self) -> None:
        self._renderTexture.display()

    def _refreshView(self) -> None:
        view = View(self._viewport)
        self._renderTexture.setView(view)
        for canvas in self._canvases:
            canvas.setView(view)
