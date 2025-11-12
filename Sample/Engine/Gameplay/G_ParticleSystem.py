# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Dict, List, Optional, Union
import copy
from . import Drawable, VertexArray, PrimitiveType, Vertex, Texture, Transform, Vector2f, Color, Text, degrees


class ParticleInfo:
    position: Vector2f
    color: Color
    rotation: float
    scale: Vector2f

    def __init__(self, position: Vector2f, color: Color, rotation: float, scale: Vector2f) -> None:
        self.position = position
        self.color = color
        self.rotation = rotation
        self.scale = scale


class ParticleBase:
    def __init__(self) -> None:
        self._parent: Optional[ParticleSystem] = None
        self._moveFunction: Optional[Callable[[float, float, Union[Particle, TextParticle]], None]] = None
        self._countTime: float = 0.0

    def setParent(self, parent: Optional[ParticleSystem]) -> None:
        self._parent = parent

    def getParent(self) -> Optional[ParticleSystem]:
        return self._parent

    def setMoveFunction(self, moveFunction: Callable[[float, float, Union[Particle, TextParticle]], None]) -> None:
        self._moveFunction = moveFunction

    def onTick(self, deltaTime: float) -> None:
        if self._moveFunction is None:
            return

        self._countTime += deltaTime
        if isinstance(self, Particle) or isinstance(self, TextParticle):
            self._moveFunction(deltaTime, self._countTime, self)

    def destroy(self) -> None:
        if self._parent is not None:
            self._parent.removeParticle(self)


class Particle(ParticleBase):
    def __init__(self, resourcePath: str, info: ParticleInfo) -> None:
        self.resourcePath = resourcePath
        self.info = info
        self._lastPosition = copy.copy(info.position)
        self._lastRotation = info.rotation
        self._lastScale = copy.copy(info.scale)
        self._lastColor = Color(info.color.toInteger())

    def onTick(self, deltaTime: float) -> None:
        if self._parent is None:
            return

        super().onTick(deltaTime)
        self._checkUpdate()

    def onLateTick(self, deltaTime: float) -> None:
        if self._parent is None:
            return

        self._checkUpdate()

    def _checkUpdate(self):
        updateFlag = False
        if self._lastPosition != self.info.position:
            self._lastPosition = copy.copy(self.info.position)
            updateFlag = True
        if self._lastRotation != self.info.rotation:
            self._lastRotation = self.info.rotation
            updateFlag = True
        if self._lastScale != self.info.scale:
            self._lastScale = copy.copy(self.info.scale)
            updateFlag = True
        if self._lastColor != self.info.color:
            self._lastColor = Color(self.info.color.toInteger())
            updateFlag = True

        if updateFlag:
            self._parent.updateParticleInfo(self)


class TextParticle(Text, ParticleBase):
    def __init__(self, font: Font, text: str, characterSize: int = 30):
        Text.__init__(self, font, text, characterSize)
        ParticleBase.__init__(self)

    def onTick(self, deltaTime: float) -> None:
        ParticleBase.onTick(self, deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        pass


class ParticleSystem(Drawable):
    def __init__(self) -> None:
        super().__init__()
        self._particles: Dict[str, List[Particle]] = {}
        self._vertexArrays: Dict[str, VertexArray] = {}
        self._resourceDict: Dict[str, Texture] = {}
        self._texts: List[Text] = []

    def addParticle(self, particle: Particle) -> None:
        if not particle.resourcePath in self._resourceDict:
            self._resourceDict[particle.resourcePath] = Texture(particle.resourcePath)
            self._particles[particle.resourcePath] = []
            self._vertexArrays[particle.resourcePath] = VertexArray(PrimitiveType.Triangles, 0)

        particle.setParent(self)
        info = particle.info
        self._particles[particle.resourcePath].append(particle)

        t = Transform()
        t.translate(info.position)
        t.rotate(degrees(info.rotation))
        t.scale(info.scale)

        width = self._resourceDict[particle.resourcePath].getSize().x
        height = self._resourceDict[particle.resourcePath].getSize().y
        halfSize = Vector2f(width / 2, height / 2)

        tl = t.transformPoint(Vector2f(-halfSize.x, -halfSize.y))
        tr = t.transformPoint(Vector2f(halfSize.x, -halfSize.y))
        br = t.transformPoint(Vector2f(halfSize.x, halfSize.y))
        bl = t.transformPoint(Vector2f(-halfSize.x, halfSize.y))

        uv_tl = Vector2f(0, 0)
        uv_tr = Vector2f(width, 0)
        uv_br = Vector2f(width, height)
        uv_bl = Vector2f(0, height)

        vertex0 = Vertex()
        vertex0.position = tl
        vertex0.texCoords = uv_tl
        vertex0.color = info.color
        vertex1 = Vertex()
        vertex1.position = tr
        vertex1.texCoords = uv_tr
        vertex1.color = info.color
        vertex2 = Vertex()
        vertex2.position = br
        vertex2.texCoords = uv_br
        vertex2.color = info.color
        vertex3 = Vertex()
        vertex3.position = tl
        vertex3.texCoords = uv_tl
        vertex3.color = info.color
        vertex4 = Vertex()
        vertex4.position = br
        vertex4.texCoords = uv_br
        vertex4.color = info.color
        vertex5 = Vertex()
        vertex5.position = bl
        vertex5.texCoords = uv_bl
        vertex5.color = info.color

        self._vertexArrays[particle.resourcePath].append(vertex0)
        self._vertexArrays[particle.resourcePath].append(vertex1)
        self._vertexArrays[particle.resourcePath].append(vertex2)
        self._vertexArrays[particle.resourcePath].append(vertex3)
        self._vertexArrays[particle.resourcePath].append(vertex4)
        self._vertexArrays[particle.resourcePath].append(vertex5)

    def addText(self, text: Text) -> None:
        self._texts.append(text)
        text.setParent(self)

    def removeText(self, text: Text) -> None:
        assert text in self._texts
        self._texts.remove(text)
        text.setParent(None)

    def removeParticle(self, particle: Particle) -> None:
        assert particle.resourcePath in self._particles
        plist = self._particles[particle.resourcePath]
        assert particle in plist
        idx = plist.index(particle)
        self.removeParticleAt(particle.resourcePath, idx)
        particle.setParent(None)

    def removeParticleAt(self, resourcePath: str, idx: int) -> None:
        plist = self._particles[resourcePath]
        va = self._vertexArrays[resourcePath]
        n_before = len(plist)

        for i in range(idx, n_before - 1):
            src = (i + 1) * 6
            dst = i * 6
            for k in range(6):
                va[dst + k].position = va[src + k].position
                va[dst + k].texCoords = va[src + k].texCoords
                va[dst + k].color = va[src + k].color

        del plist[idx]
        va.resize((n_before - 1) * 6)
        if n_before - 1 == 0:
            self._particles.pop(resourcePath)
            self._vertexArrays.pop(resourcePath)
            self._resourceDict.pop(resourcePath)

    def updateParticleInfo(self, particle: Particle) -> None:
        assert particle.resourcePath in self._particles
        assert particle in self._particles[particle.resourcePath]
        idx = self._particles[particle.resourcePath].index(particle)
        t = Transform()
        t.translate(particle.info.position)
        t.rotate(degrees(particle.info.rotation))
        t.scale(particle.info.scale)

        width = self._resourceDict[particle.resourcePath].getSize().x
        height = self._resourceDict[particle.resourcePath].getSize().y
        halfSize = Vector2f(width / 2, height / 2)

        tl = t.transformPoint(Vector2f(-halfSize.x, -halfSize.y))
        tr = t.transformPoint(Vector2f(halfSize.x, -halfSize.y))
        br = t.transformPoint(Vector2f(halfSize.x, halfSize.y))
        bl = t.transformPoint(Vector2f(-halfSize.x, halfSize.y))

        self._vertexArrays[particle.resourcePath][idx * 6 + 0].position = tl
        self._vertexArrays[particle.resourcePath][idx * 6 + 1].position = tr
        self._vertexArrays[particle.resourcePath][idx * 6 + 2].position = br
        self._vertexArrays[particle.resourcePath][idx * 6 + 3].position = tl
        self._vertexArrays[particle.resourcePath][idx * 6 + 4].position = br
        self._vertexArrays[particle.resourcePath][idx * 6 + 5].position = bl
        self._vertexArrays[particle.resourcePath][idx * 6 + 0].color = particle.info.color
        self._vertexArrays[particle.resourcePath][idx * 6 + 1].color = particle.info.color
        self._vertexArrays[particle.resourcePath][idx * 6 + 2].color = particle.info.color
        self._vertexArrays[particle.resourcePath][idx * 6 + 3].color = particle.info.color
        self._vertexArrays[particle.resourcePath][idx * 6 + 4].color = particle.info.color
        self._vertexArrays[particle.resourcePath][idx * 6 + 5].color = particle.info.color

    def onTick(self, deltaTime: float) -> None:
        for _, plist in self._particles.items():
            for particle in plist:
                particle.onTick(deltaTime)
        for text in self._texts:
            if isinstance(text, TextParticle):
                text.onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        for _, plist in self._particles.items():
            for particle in plist:
                particle.onLateTick(deltaTime)
        for text in self._texts:
            if isinstance(text, TextParticle):
                text.onLateTick(deltaTime)

    def draw(self, target: RenderTarget, state: RenderStates) -> None:
        originTexture = state.texture
        for resourcePath, vertexArray in self._vertexArrays.items():
            state.texture = self._resourceDict[resourcePath]
            target.draw(vertexArray, state)
        state.texture = originTexture
        for text in self._texts:
            text.draw(target, state)
