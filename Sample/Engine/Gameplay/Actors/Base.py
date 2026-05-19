# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
import os
import warnings
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from ... import (
    Pair,
    Sprite,
    IntRect,
    Vector2i,
    Vector2u,
    Vector2f,
    Shader,
    Angle,
    degrees,
    Utils,
)
from ...Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce
from ..Material import Material

if TYPE_CHECKING:
    from Engine import Texture
    from Global import GameMap
    from Engine.NodeGraph import Graph


class _ActorBase(Sprite):
    """Base class for all scene entities.

    Extends the SFML Sprite with transform hierarchy, material system,
    texture animation, and child-parent relationships.
    Not intended to be instantiated directly; use `Actor` instead.
    """

    tag: str = ""  #: Identifier tag for lookups
    switchInterval: float = 0.2  #: Frame switch interval for sprite animation (seconds)
    animatable: bool = False  #: Whether sprite-sheet animation is enabled
    material: Material = Material()  #: Surface material (lighting, speed, opacity)
    shaderPath: str = ""  #: Path to a fragment shader applied to this actor

    @TypeAdapter(rect=([tuple, list], IntRect, lambda pos, size: IntRect(Vector2i(*pos), Vector2i(*size))))
    def __init__(
        self,
        texture: Optional[Texture] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]], List[List[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""Construct an actor base from a texture and optional sub-rectangle.

        - \param texture  Source texture (or `None` for an invisible actor)
        - \param rect     Sub-rectangle of the texture to display
        - \param tag      Optional identifier tag
        """
        args = [texture]
        if not rect is None:
            args.append(rect)
        super().__init__(*args)

        if not tag is None:
            self.tag = tag
        self._map: Optional[GameMap] = None
        self._parent: Optional[_ActorBase] = None
        self._children: List[_ActorBase] = []
        self._translation: Vector2f = Vector2f(0, 0)
        self._visible: bool = True
        self._texture: Optional[Texture] = texture
        self._switchTimer: float = 0.0
        self._relativePosition: Vector2f = Vector2f(0, 0)
        self._relativeRotation: Angle = degrees(0)
        self._relativeScale: Vector2f = Vector2f(1, 1)
        self._graph: Optional[Graph] = None
        self._shader: Optional[Shader] = None
        self._shaderError: bool = False
        self._loadShader()

    def __eq__(self, other: object) -> bool:
        return self is other

    def __ne__(self, other: object) -> bool:
        return self is not other

    def _loadShader(self) -> None:
        self._shader = None
        self._shaderError = False
        if self.shaderPath:
            if IS_IOS_PLATFORM:
                warnIosShaderSkippedOnce(
                    f"Actor.shaderPath:{self.shaderPath}",
                    f"iOS: shaders are disabled; skipped actor shader: {self.shaderPath}",
                )
                return
            try:
                # shaderPath is relative to Assets/Shaders/
                if self.shaderPath.startswith("Assets/Shaders/"):
                    fullPath = self.shaderPath
                else:
                    fullPath = os.path.join(".", "Assets", "Shaders", self.shaderPath)

                if os.path.exists(fullPath):
                    self._shader = Shader(fullPath, Shader.Type.Fragment)
                else:
                    self._shaderError = True
                    print(f"Warning: Shader file not found: {fullPath}")
            except Exception as e:
                self._shaderError = True
                print(f"Warning: Shader load failed for {self.shaderPath}: {e}")

    @ExecSplit(default=(None,))
    def setShaderPath(self, shaderPath: str) -> None:
        r"""\brief Set the shader path and load the shader.

        - \param shaderPath Path to the fragment shader (relative to Assets/Shaders/)
        """
        self.shaderPath = shaderPath
        self._loadShader()

    @ReturnType(shaderPath=str)
    def getShaderPath(self) -> str:
        r"""\brief Get the current shader path.

        - \return The shader path string
        """
        return self.shaderPath

    @ReturnType(shader=Optional[Shader])
    def getShader(self) -> Optional[Shader]:
        r"""\brief Get the loaded shader object.

        - \return The shader object, or None if not loaded
        """
        return self._shader

    @ReturnType(error=bool)
    def hasShaderError(self) -> bool:
        r"""\brief Check if the shader failed to load.

        - \return True if the shader failed to load, False otherwise
        """
        return self._shaderError

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the actor (called every frame).

        Handles sprite-sheet animation if animatable is enabled.

        - \param deltaTime Time elapsed since last frame (in seconds)
        """
        if self.animatable:
            self._animate(deltaTime)

    def lateUpdate(self, deltaTime: float) -> None:
        r"""\brief Late update callback (called after all actors are updated).

        Override in subclasses to implement post-update logic.

        - \param deltaTime Time elapsed since last frame (in seconds)
        """
        pass

    def fixedUpdate(self, fixedDelta: float) -> None:
        r"""\brief Fixed timestep update callback.

        Override this method to implement physics or movement logic
        that requires a constant timestep.

        - \param fixedDelta Fixed time step interval (in seconds)
        """
        pass

    @ReturnType(pos=Vector2f)
    def getPosition(self) -> Vector2f:
        r"""\brief Get the world position.

        - \return World position as Vector2f (excluding translation offset)
        """
        return super().getPosition() - self._translation

    @ReturnType(pos=Pair[float])
    def v_getPosition(self) -> Pair[float]:
        r"""\brief Get the world position as a raw tuple.

        - \return Position as a tuple of (x, y) floats
        """
        return super().getPosition().unpack()

    @ReturnType(pos=Vector2f)
    def getRelativePosition(self) -> Vector2f:
        r"""\brief Get the position relative to the parent actor.

        - \return Relative position as Vector2f
        """
        return self._relativePosition

    @ReturnType(pos=Pair[float])
    def v_getRelativePosition(self) -> Pair[float]:
        r"""\brief Get the relative position as a raw tuple.

        - \return Relative position as a tuple of (x, y) floats
        """
        return self._relativePosition.unpack()

    @ReturnType(pos=Vector2i)
    def getMapPosition(self) -> Vector2i:
        r"""\brief Get the grid cell position.

        Calculates the grid cell by dividing world position by CellSize.

        - \return Grid cell position as Vector2i
        """
        from ... import CellSize

        return Vector2i(
            int(self.getPosition().x * 1.0 / CellSize + 0.5),
            int(self.getPosition().y * 1.0 / CellSize + 0.5),
        )

    @ReturnType(pos=Pair[int])
    def v_getMapPosition(self) -> Pair[int]:
        r"""\brief Get the grid cell position as a raw tuple.

        - \return Grid cell position as a tuple of (x, y) integers
        """
        return self.getMapPosition().unpack()

    @ReturnType(pos=Vector2i)
    def getRelativeMapPosition(self) -> Vector2i:
        r"""\brief Get the grid cell position relative to the parent.

        - \return Relative grid cell position as Vector2i
        """
        from ... import CellSize

        return Vector2i(
            int(self.getRelativePosition().x / CellSize),
            int(self.getRelativePosition().y / CellSize),
        )

    @ReturnType(pos=Pair[int])
    def v_getRelativeMapPosition(self) -> Pair[int]:
        r"""\brief Get the relative grid cell position as a raw tuple.

        - \return Relative grid cell position as a tuple of (x, y) integers
        """
        return self.getRelativeMapPosition().unpack()

    @ExecSplit(default=(None,))
    @TypeAdapter(position=([tuple, list], Vector2f))
    def setPosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the absolute world position.

        Updates the relative position based on parent, and propagates
        to all children.

        - \param position New world position as Vector2f, tuple, or list
        """
        parent = self.getParent()
        if parent:
            parentPosition = parent.getPosition()
            self._relativePosition = position - parentPosition
        else:
            self._relativePosition = Vector2f(0, 0)
        super().setPosition(position + self._translation)
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()

    @ExecSplit(default=(None,))
    @TypeAdapter(position=([tuple, list], Vector2f))
    def setRelativePosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the position relative to the parent actor.

        - \param position Relative position as Vector2f, tuple, or list
        """
        parentPosition = Vector2f(0, 0)
        parent = self.getParent()
        if parent:
            parentPosition = parent.getPosition()
        self.setPosition(parentPosition + position)

    @ExecSplit(default=(None,))
    @TypeAdapter(position=([tuple, list], Vector2u))
    def setMapPosition(self, position: Union[Vector2u, Pair[int], List[int]]) -> None:
        r"""\brief Set position by grid cell coordinates.

        - \param position Grid cell position as Vector2u, tuple, or list
        """
        from ... import CellSize

        self.setPosition(Vector2f(position.x * CellSize, position.y * CellSize))

    @ExecSplit(default=(None,))
    @TypeAdapter(position=([tuple, list], Vector2u))
    def setRelativeMapPosition(self, position: Union[Vector2u, Pair[int], List[int]]) -> None:
        r"""\brief Set relative position by grid cell coordinates.

        - \param position Relative grid cell position as Vector2u, tuple, or list
        """
        from ... import CellSize

        self.setRelativePosition(Vector2f(position.x * CellSize, position.y * CellSize))

    @ExecSplit(default=(None,))
    @TypeAdapter(offset=([tuple, list], Vector2f))
    def move(self, offset: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Move by a pixel offset.

        Propagates the movement to all children.

        - \param offset Movement offset as Vector2f, tuple, or list
        """
        super().move(offset)
        self._relativePosition += offset
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()

    @ReturnType(angle=Angle)
    def getRotation(self) -> Angle:
        r"""\brief Get the current rotation angle.

        - \return Rotation angle as Angle object
        """
        return super().getRotation()

    @ReturnType(angle=float)
    def v_getRotation(self) -> float:
        r"""\brief Get the current rotation in degrees.

        - \return Rotation angle in degrees as a float
        """
        return super().getRotation().asDegrees()

    @ReturnType(angle=Angle)
    def getRelativeRotation(self) -> Angle:
        r"""\brief Get the rotation relative to the parent.

        - \return Relative rotation as Angle object
        """
        return self._relativeRotation

    @ReturnType(angle=float)
    def v_getRelativeRotation(self) -> float:
        r"""\brief Get the relative rotation in degrees.

        - \return Relative rotation in degrees as a float
        """
        return self._relativeRotation.asDegrees()

    @ExecSplit(default=(None,))
    def setRotation(self, angle: Union[Angle, float]) -> None:
        r"""\brief Set the absolute rotation.

        Updates the relative rotation based on parent, and propagates
        to all children.

        - \param angle Rotation angle (Angle object or float in degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        parent = self.getParent()
        if parent:
            parentRotation = parent.getRotation()
            self._relativeRotation = angle - parentRotation
        else:
            self._relativeRotation = degrees(0)
        super().setRotation(angle)
        if self.getChildren():
            for child in self.getChildren():
                child._updateRotationFromParent()

    @ExecSplit(default=(None,))
    def rotate(self, angle: Union[Angle, float]) -> None:
        r"""\brief Rotate by a relative angle.

        Adds the angle to the current rotation and propagates to children.

        - \param angle Relative rotation angle (Angle object or float in degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        self._relativeRotation += angle
        super().rotate(angle)
        if self.getChildren():
            for child in self.getChildren():
                child._updateRotationFromParent()

    @ExecSplit(default=(None,))
    def setRelativeRotation(self, angle: Union[Angle, float]) -> None:
        r"""\brief Set the rotation relative to the parent.

        - \param angle Relative rotation angle (Angle object or float in degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        parentRotation = degrees(0)
        parent = self.getParent()
        if parent:
            parentRotation = parent.getRotation()
        self.setRotation(parentRotation + angle)

    @ReturnType(scale=Pair[float])
    def v_getScale(self) -> Pair[float]:
        r"""\brief Get the scale as a raw tuple.

        - \return Scale as a tuple of (x, y) floats
        """
        return super().getScale().unpack()

    @ReturnType(scale=Vector2f)
    def getScale(self) -> Vector2f:
        r"""\brief Get the current scale.

        \return Scale as Vector2f
        """
        return super().getScale()

    @ReturnType(scale=Vector2f)
    def getRelativeScale(self) -> Vector2f:
        r"""\brief Get the scale relative to the parent.

        - \return Relative scale as Vector2f
        """
        return self._relativeScale

    @ReturnType(scale=Pair[float])
    def v_getRelativeScale(self) -> Pair[float]:
        r"""\brief Get the relative scale as a raw tuple.

        - \return Relative scale as a tuple of (x, y) floats
        """
        return self._relativeScale.unpack()

    @ExecSplit(default=(None,))
    @TypeAdapter(factors=([tuple, list], Vector2f))
    def setScale(self, factors: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the absolute scale.

        Updates the relative scale based on parent, and propagates
        to all children.

        - \param factors New scale factors as Vector2f, tuple, or list
        """
        parent = self.getParent()
        if parent:
            parentScale = parent.getScale()
            self._relativeScale = factors.componentWiseDiv(parentScale)
        else:
            self._relativeScale = Vector2f(1, 1)
        super().setScale(factors)
        if self.getChildren():
            for child in self.getChildren():
                child._updateScaleFromParent()

    @ExecSplit(default=(None,))
    @TypeAdapter(factor=([tuple, list], Vector2f))
    def scale(self, factor: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Scale by a relative factor.

        Multiplies the current scale by the given factor and propagates
        to all children.

        - \param factor Scale factor as Vector2f, tuple, or list
        """
        self._relativeScale = self._relativeScale.componentWiseMul(factor)
        super().scale(factor)
        if self.getChildren():
            for child in self.getChildren():
                child._updateScaleFromParent()

    @ExecSplit(default=(None,))
    @TypeAdapter(scale=([tuple, list], Vector2f))
    def setRelativeScale(self, scale: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the scale relative to the parent.

        - \param scale Relative scale as Vector2f, tuple, or list
        """
        parentScale = Vector2f(1, 1)
        parent = self.getParent()
        if parent:
            parentScale = parent.getScale()
        self.setScale(parentScale.componentWiseMul(scale))

    @ReturnType(origin=Pair[float])
    def v_getOrigin(self) -> Pair[float]:
        r"""\brief Get the origin as a raw tuple.

        - \return Origin as a tuple of (x, y) floats
        """
        return super().getOrigin().unpack()

    @ReturnType(origin=Vector2f)
    def getOrigin(self) -> Vector2f:
        r"""\brief Get the current origin.

        - \return Origin point as Vector2f
        """
        return super().getOrigin()

    @ExecSplit(default=(None,))
    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the origin point.

        The origin is the point around which transformations (position,
        rotation, scale) are applied.

        - \param origin New origin point as Vector2f, tuple, or list
        """
        return super().setOrigin(origin)

    @ReturnType(translation=Vector2f)
    def getTranslation(self) -> Vector2f:
        r"""\brief Get the translation offset.

        - \return Translation offset as Vector2f
        """
        return self._translation

    @ExecSplit(default=(None,))
    @TypeAdapter(translation=([tuple, list], Vector2f))
    def setTranslation(self, translation: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the translation offset.

        The translation is an additional offset applied to the position.
        After setting, the actor's position is recalculated.

        - \param translation Translation offset as Vector2f, tuple, or list
        """
        self._translation = translation
        self.setPosition(self.getPosition())

    @ExecSplit(default=(None,))
    def setAlignment(self, alignment: Tuple) -> None:
        r"""\brief Set the origin based on alignment ratios.

        Sets the origin point as a ratio of the texture rectangle size.
        (0, 0) = top-left, (0.5, 0.5) = centre, (1, 1) = bottom-right.

        - \param alignment Tuple of (x, y) ratios, each in range [0, 1]
        """
        x, y = alignment
        x = Utils.Math.Clamp(x, 0, 1)
        y = Utils.Math.Clamp(y, 0, 1)
        size = self.getTextureRect().size
        origin = Vector2f(size.x * x, size.y * y)
        self.setOrigin(origin)

    @ReturnType(map_="GameMap")
    def getMap(self) -> Optional[GameMap]:
        r"""\brief Get the map that this actor belongs to.

        - \return The GameMap instance, or None if not assigned
        """
        return self._map

    @ExecSplit(default=(None,))
    def setMap(self, inMap: GameMap) -> None:
        r"""\brief Set the map that this actor belongs to.

        - \param inMap The GameMap instance to assign
        """
        self._map = inMap

    @ReturnType(parent=Optional["_ActorBase"])
    def getParent(self) -> Optional[_ActorBase]:
        r"""\brief Get the parent actor in the hierarchy.

        - \return Parent actor, or None if this is the root
        """
        return self._parent

    @ExecSplit(default=(None,))
    def setParent(self, parent: Optional[_ActorBase]) -> None:
        r"""\brief Set the parent actor.

        - \param parent The parent actor to assign, or None to detach
        """
        self._parent = parent

    @ReturnType(children=List["_ActorBase"])
    def getChildren(self) -> List[_ActorBase]:
        r"""\brief Get the list of child actors.

        - \return List of child actors
        """
        return self._children

    @ExecSplit(default=(None,))
    def addChild(self, child: _ActorBase) -> None:
        r"""\brief Attach a child actor to this actor's hierarchy.

        Sets the child's parent and map, then updates the actor list
        in the map if applicable.

        - \param child The child actor to attach
        """
        if child in self._children:
            warnings.warn("Child already exists")
            return
        self._children.append(child)
        child.setParent(self)
        if self._map:
            child.setMap(self._map)
            self._map.updateActorList()

    @ExecSplit(default=(None,))
    def removeChild(self, child: _ActorBase) -> None:
        r"""\brief Remove a child actor from this actor's hierarchy.

        - \param child The child actor to remove

        - \throw ValueError If the child is not found in the hierarchy
        """
        if child not in self._children:
            raise ValueError("Child not found")
        self._children.remove(child)

    @ReturnType(visible=bool)
    def getVisible(self) -> bool:
        r"""\brief Get the visibility state of this actor.

        - \return True if visible, False if hidden
        """
        return self._visible

    @ExecSplit(default=(None,))
    def setVisible(self, visible: bool, applyToChildren: bool = True) -> None:
        r"""\brief Set the visibility of this actor.

        Optionally propagates the visibility setting to all children.

        - \param visible True to make visible, False to hide
        - \param applyToChildren If True, apply to all children recursively
        """
        self._visible = visible
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setVisible(visible, applyToChildren)

    @ReturnType(animatable=bool)
    def getAnimatable(self) -> bool:
        r"""\brief Get the animation state of this actor.

        - \return True if animation is enabled, False otherwise
        """
        return self.animatable

    @ExecSplit(default=(None,))
    def setAnimatable(self, animate: bool, applyToChildren: bool = True) -> None:
        r"""\brief Enable or disable sprite-sheet animation.

        Optionally propagates the setting to all children.

        - \param animate True to enable animation, False to disable
        - \param applyToChildren If True, apply to all children recursively
        """
        self.animatable = animate
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setAnimatable(animate, applyToChildren)

    @ReturnType(texture=Optional["Texture"])
    def getSpriteTexture(self) -> Optional[Texture]:
        r"""\brief Get the texture of the underlying SFML sprite.

        - \return The texture, or None if not set
        """
        return super().getTexture()

    @ReturnType(texture=Optional["Texture"])
    def getTexture(self) -> Optional[Texture]:
        r"""\brief Get the texture reference of this actor.

        - \return The texture reference, or None if not set
        """
        return self._texture

    @ExecSplit(default=(None,))
    def setSpriteTexture(self, texture: Texture, resetRect: bool = False) -> None:
        r"""\brief Set the texture for the underlying SFML sprite.

        This only updates the sprite's texture, not the actor's texture reference.

        - \param texture The texture to apply
        - \param resetRect If True, reset the texture rectangle to the texture size
        """
        super().setTexture(texture, resetRect)

    @ExecSplit(default=(None,))
    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        r"""\brief Set the texture for this actor.

        Updates both the internal texture reference and the sprite's texture.

        - \param texture The texture to apply
        - \param resetRect If True, reset the texture rectangle to the texture size
        """
        self._texture = texture
        self.setSpriteTexture(texture, resetRect)

    @ReturnType(material=Material)
    def getMaterial(self) -> Material:
        r"""\brief Get the material of this actor.

        - \return The Material instance
        """
        return self.material

    @ExecSplit(default=(None,))
    def setMaterial(self, material: Material) -> None:
        r"""\brief Set the material for this actor.

        The material defines lighting, opacity, and other surface properties.

        - \param material The Material to apply
        """
        self.material = material

    @ReturnType(lightBlock=float)
    def getLightBlock(self) -> float:
        r"""\brief Get the light blocking factor.

        - \return Light blocking factor (0.0 to 1.0)
        """
        return self.material.lightBlock

    @ExecSplit(default=(None,))
    def setLightBlock(self, lightBlock: float) -> None:
        r"""\brief Set the light blocking factor.

        Controls how much light is blocked by this actor's material.

        - \param lightBlock Light blocking factor (typically 0.0 to 1.0)
        """
        self.material.lightBlock = lightBlock

    @ReturnType(mirror=bool)
    def getMirror(self) -> bool:
        r"""\brief Get the mirror property of the material.

        - \return True if mirror effect is enabled, False otherwise
        """
        return self.material.mirror

    @ExecSplit(default=(None,))
    def setMirror(self, mirror: bool) -> None:
        r"""\brief Set the mirror property of the material.

        Controls whether the actor's surface acts as a mirror.

        - \param mirror True to enable mirror effect, False to disable
        """
        self.material.mirror = mirror

    @ReturnType(reflectionStrength=float)
    def getReflectionStrength(self) -> float:
        r"""\brief Get the reflection strength of the material.

        - \return Reflection strength factor (0.0 to 1.0)
        """
        return self.material.reflectionStrength

    @ExecSplit(default=(None,))
    def setReflectionStrength(self, reflectionStrength: float) -> None:
        r"""\brief Set the reflection strength of the material.

        Controls how strongly the actor's surface reflects light.

        - \param reflectionStrength Reflection strength factor (0.0 to 1.0)
        """
        self.material.reflectionStrength = reflectionStrength

    @ReturnType(opacity=float)
    def getOpacity(self) -> float:
        r"""\brief Get the opacity of the material.

        - \return Opacity factor in range [0.0, 1.0]
        """
        return self.material.opacity

    @ExecSplit(default=(None,))
    def setOpacity(self, opacity: float) -> None:
        r"""\brief Set the opacity of the material.

        Controls the transparency of the actor (0.0 = fully transparent,
        1.0 = fully opaque).

        - \param opacity Opacity factor in range [0.0, 1.0]
        """
        self.material.opacity = opacity

    @ReturnType(emissive=float)
    def getEmissive(self) -> float:
        r"""\brief Get the emissive property of the material.

        - \return Emissive factor (0.0 = no emission)
        """
        return self.material.emissive

    @ExecSplit(default=(None,))
    def setEmissive(self, emissive: float) -> None:
        r"""\brief Set the emissive property of the material.

        Controls how much light the actor emits.

        - \param emissive Emissive factor (0.0 = no emission)
        """
        self.material.emissive = emissive

    @ExecSplit(default=(None,))
    def setGraph(self, graph: Graph) -> None:
        r"""\brief Set the behaviour graph for this actor.

        The graph drives the actor's logic and state machine.

        - \param graph The Graph instance to assign
        """
        self._graph = graph

    @TypeAdapter(offset=([tuple, list], Vector2f))
    def _superMove(self, offset: Union[Vector2f, Pair[float], List[float]]) -> None:
        super().move(offset)

    def _updatePositionFromParent(self) -> None:
        parent = self.getParent()
        if parent:
            parentPosition = parent.getPosition()
            newPosition = parentPosition + self._relativePosition
            super().setPosition(newPosition)
            if self.getChildren():
                for child in self.getChildren():
                    child._updatePositionFromParent()

    def _updateRotationFromParent(self) -> None:
        parent = self.getParent()
        if parent:
            parentRotation = parent.getRotation()
            newRotation = parentRotation + self._relativeRotation
            super().setRotation(newRotation)
            if self.getChildren():
                for child in self.getChildren():
                    child._updateRotationFromParent()

    def _updateScaleFromParent(self) -> None:
        parent = self.getParent()
        if parent:
            parentScale = parent.getScale()
            newScale = parentScale.componentWiseMul(self._relativeScale)
            super().setScale(newScale)
            if self.getChildren():
                for child in self.getChildren():
                    child._updateScaleFromParent()

    def _animate(self, deltaTime: float) -> None:
        newRect = copy.copy(self.getTextureRect())
        rectWidth = newRect.size.x
        rectX = newRect.position.x
        if self._texture:
            textureWidth = self._texture.getSize().x
            newRectX = (rectX + rectWidth) % textureWidth
            newRect.position.x = newRectX
            self._switchTimer += deltaTime
            if self._switchTimer >= self.switchInterval:
                self._switchTimer = 0.0
                self.setTextureRect(newRect)
