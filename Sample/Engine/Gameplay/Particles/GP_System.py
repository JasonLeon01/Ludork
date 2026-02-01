# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict, List, TYPE_CHECKING, Tuple
from ... import Drawable, VertexArray, PrimitiveType, Vertex, Texture, Transform, Vector2f
from .GP_Particle import Particle
from .GP_Text import TextParticle

if TYPE_CHECKING:
    from Engine import RenderTarget, RenderStates


class System(Drawable):
    def __init__(self) -> None:
        super().__init__()
        self._particles: Dict[str, List[Particle]] = {}
        self._vertexArrays: Dict[str, VertexArray] = {}
        self._resourceDict: Dict[str, Texture] = {}
        self._texts: List[TextParticle] = []
        self._textureUV: Dict[str, Tuple[int, int, Vector2f, Vector2f, Vector2f, Vector2f]] = {}
        self._updateFlags: List[Particle] = []

    def addParticle(self, particle: Particle) -> None:
        if not particle.resourcePath in self._resourceDict:
            texture = Texture(particle.resourcePath)
            self._resourceDict[particle.resourcePath] = texture
            self._particles[particle.resourcePath] = []
            self._vertexArrays[particle.resourcePath] = VertexArray(PrimitiveType.Triangles, 0)
            width = texture.getSize().x
            height = texture.getSize().y
            self._textureUV[particle.resourcePath] = (
                width,
                height,
                Vector2f(0, 0),
                Vector2f(width, 0),
                Vector2f(width, height),
                Vector2f(0, height),
            )

        particle.setParent(self)
        info = particle.info
        self._particles[particle.resourcePath].append(particle)
        width, height, uv_tl, uv_tr, uv_br, uv_bl = self._textureUV[particle.resourcePath]
        halfSize = Vector2f(width / 2, height / 2)
        tl_tr = Vector2f(-halfSize.x, -halfSize.y)
        tr_tr = Vector2f(halfSize.x, -halfSize.y)
        br_tr = Vector2f(halfSize.x, halfSize.y)
        bl_tr = Vector2f(-halfSize.x, halfSize.y)

        try:
            from ..GamePlayExtension import C_AddParticle

            C_AddParticle(
                info.position,
                info.rotation,
                info.scale,
                info.color,
                uv_tl,
                uv_tr,
                uv_br,
                uv_bl,
                tl_tr,
                tr_tr,
                br_tr,
                bl_tr,
                self._vertexArrays[particle.resourcePath],
            )
        except Exception as e:
            # region Add Particle by Python
            print(f"Failed to add particle by C extension, try to add by python. Error: {e}")
            t = Transform()
            t.translate(info.position)
            t.rotate(info.rotation)
            t.scale(info.scale)

            tl = t.transformPoint(tl_tr)
            tr = t.transformPoint(tr_tr)
            br = t.transformPoint(br_tr)
            bl = t.transformPoint(bl_tr)

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
            # endregion

    def addText(self, text: TextParticle) -> None:
        self._texts.append(text)
        text.setParent(self)

    def removeText(self, text: TextParticle) -> None:
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
        isParticleListEmpty = False
        try:
            from ..GamePlayExtension import C_RemoveParticle

            isParticleListEmpty = C_RemoveParticle(plist, va, idx)
        except Exception as e:
            # region Remove Particle by Python
            print(f"Failed to remove particle by C extension, try to remove by python. Error: {e}")
            n_before = len(plist)

            if idx != n_before - 1:
                for i in range(idx, n_before - 1):
                    src = (i + 1) * 6
                    dst = i * 6
                    for k in range(6):
                        va[dst + k] = va[src + k]
            del plist[idx]
            va.resize((n_before - 1) * 6)
            isParticleListEmpty = n_before == 1
            # endregion

        if isParticleListEmpty:
            self._particles.pop(resourcePath)
            self._vertexArrays.pop(resourcePath)
            self._resourceDict.pop(resourcePath)
            self._textureUV.pop(resourcePath)

    def addUpdateFlag(self, particle: Particle) -> None:
        self._updateFlags.append(particle)

    def updateParticlesInfo(self) -> None:
        def getUpdateParticleInfo(particle):
            width, height, _, _, _, _ = self._textureUV[particle.resourcePath]
            halfSize = Vector2f(width / 2, height / 2)
            tl_tr = Vector2f(-halfSize.x, -halfSize.y)
            tr_tr = Vector2f(halfSize.x, -halfSize.y)
            br_tr = Vector2f(halfSize.x, halfSize.y)
            bl_tr = Vector2f(-halfSize.x, halfSize.y)
            return tl_tr, tr_tr, br_tr, bl_tr

        try:
            from ..GamePlayExtension import C_UpdateParticlesInfo

            C_UpdateParticlesInfo(getUpdateParticleInfo, self._updateFlags, self._particles, self._vertexArrays)
        except Exception as e:
            # region Update Particle Info by Python
            print(f"Failed to update particles info by C extension, try to update by python. Error: {e}")
            for particle in self._updateFlags:
                assert particle.resourcePath in self._particles
                assert particle in self._particles[particle.resourcePath]
                idx = self._particles[particle.resourcePath].index(particle)
                t = Transform()
                t.translate(particle.info.position)
                t.rotate(particle.info.rotation)
                t.scale(particle.info.scale)

                tl_tr, tr_tr, br_tr, bl_tr = getUpdateParticleInfo(particle)
                tl = t.transformPoint(tl_tr)
                tr = t.transformPoint(tr_tr)
                br = t.transformPoint(br_tr)
                bl = t.transformPoint(bl_tr)

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
            # endregion

    def onTick(self, deltaTime: float) -> None:
        for _, plist in self._particles.items():
            for particle in plist:
                particle.onTick(deltaTime)
        for text in self._texts:
            text.onTick(deltaTime)
        if len(self._updateFlags) > 0:
            self.updateParticlesInfo()
            self._updateFlags.clear()

    def onLateTick(self, deltaTime: float) -> None:
        for _, plist in self._particles.items():
            for particle in plist:
                particle.onLateTick(deltaTime)
        for text in self._texts:
            text.onLateTick(deltaTime)

    def onFixedTick(self, fixedDelta: float) -> None:
        for _, plist in self._particles.items():
            for particle in plist:
                particle.onFixedTick(fixedDelta)
        for text in self._texts:
            text.onFixedTick(fixedDelta)

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        originTexture = states.texture
        for resourcePath, vertexArray in self._vertexArrays.items():
            states.texture = self._resourceDict[resourcePath]
            target.draw(vertexArray, states)
        states.texture = originTexture
        for text in self._texts:
            target.draw(text)
