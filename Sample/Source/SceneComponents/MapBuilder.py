# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import TYPE_CHECKING, Any, Dict, List, Optional, Tuple

import Engine
from Engine import Color, RectangleShape, RenderTexture, Texture, Vector2f, Vector2u, View
from Engine import TileLayerData
from Engine.Gameplay import TileLayer, Tilemap
from Engine.Utils import File, Render
from Global import Camera, GameMap, Light, Manager
from Source import Data, System

if TYPE_CHECKING:
    from Source.GameInstance import GameInstance


_MAP_DATA_ROOT = os.path.join(".", "Data", "Maps")
_MAP_DATA_EXTENSIONS = (".dat", ".json")


class SceneMapBuilder:
    r"""\brief Build map runtime objects and floor-map previews for SceneMap."""

    def resolveMapPath(self, mapPath: str, currentMap: Optional[str] = None) -> str:
        r"""\brief Resolve a map key or file path to an existing map data file.

        - \param mapPath Map key or map file path.
        - \param currentMap Current map path used to inherit extension.
        - \return Resolved map file path relative to ``Data/Maps``.
        """
        candidates = self._getMapPathCandidates(mapPath, currentMap)
        for candidate in candidates:
            if os.path.exists(self.getMapDataPath(candidate)):
                return candidate
        return candidates[0] if candidates else self._normaliseMapPath(mapPath)

    def loadMapData(self, mapPath: str, currentMap: Optional[str] = None) -> Tuple[str, Dict[str, Any]]:
        r"""\brief Load map data from either JSON or .dat format.

        - \param mapPath Map key or map file path.
        - \param currentMap Current map path used to inherit extension.
        - \return Resolved map path and loaded map data.
        """
        resolvedPath = self.resolveMapPath(mapPath, currentMap)
        ext = os.path.splitext(resolvedPath)[1].lower()
        fullPath = self.getMapDataPath(resolvedPath)
        if ext == ".json":
            data = File.getJSONData(fullPath)
        elif ext == ".dat":
            data = File.loadData(fullPath)
        else:
            raise ValueError(f"Unsupported map data format: {resolvedPath}")
        if not isinstance(data, dict):
            raise TypeError(f"Map data must be a dictionary: {resolvedPath}")
        return resolvedPath, data

    def getMapDataPath(self, mapPath: str) -> str:
        r"""\brief Convert a map path to an on-disk path under ``Data/Maps``.

        - \param mapPath Map file path relative to ``Data/Maps``.
        - \return On-disk map data path.
        """
        return os.path.join(_MAP_DATA_ROOT, self._normaliseMapPath(mapPath).replace("/", os.sep))

    def _getMapPathCandidates(self, mapPath: str, currentMap: Optional[str]) -> List[str]:
        mapPath = self._normaliseMapPath(mapPath)
        if not mapPath:
            return []
        candidates: List[str] = []
        ext = os.path.splitext(mapPath)[1].lower()
        if ext:
            candidates.append(mapPath)
            if ext in _MAP_DATA_EXTENSIONS:
                stem = mapPath[: -len(ext)]
                for candidateExt in _MAP_DATA_EXTENSIONS:
                    candidates.append(f"{stem}{candidateExt}")
        else:
            currentExt = os.path.splitext(self._normaliseMapPath(currentMap or System.getStartMap()))[1].lower()
            if currentExt in _MAP_DATA_EXTENSIONS:
                candidates.append(f"{mapPath}{currentExt}")
            for candidateExt in _MAP_DATA_EXTENSIONS:
                candidates.append(f"{mapPath}{candidateExt}")
        return list(dict.fromkeys(candidates))

    def _normaliseMapPath(self, mapPath: Optional[str]) -> str:
        if mapPath is None:
            return ""
        path = str(mapPath).replace("\\", "/")
        while path.startswith("./"):
            path = path[2:]
        marker = "Data/Maps/"
        markerIndex = path.find(marker)
        if markerIndex != -1:
            path = path[markerIndex + len(marker) :]
        return path

    def generateTilemap(self, data: Dict[str, Any], width: int, height: int) -> Tilemap:
        r"""\brief Generate a tilemap from map layer data.

        - \param data Map layer data.
        - \param width Map width in tiles.
        - \param height Map height in tiles.
        - \return Generated tilemap.
        """
        mapLayers = []
        for layerData in data.values():
            name = layerData["layerName"]
            layerTileset = Data.getTileset(layerData["layerTileset"])
            layerTiles = layerData["tiles"]
            tiles: List[List[Optional[int]]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tileNumber = layerTiles[y][x]
                    tiles[-1].append(tileNumber)
            rawAutoTiles = layerData.get("autoTiles")
            autoTilePool = []
            autoTileIndexByKey: Dict[str, int] = {}
            autoTileGrid: List[List[Optional[int]]] = []
            if isinstance(rawAutoTiles, list):
                for y in range(height):
                    row: List[Optional[int]] = []
                    rawRow = rawAutoTiles[y] if y < len(rawAutoTiles) else None
                    for x in range(width):
                        cell = rawRow[x] if isinstance(rawRow, list) and x < len(rawRow) else None
                        if isinstance(cell, str) and cell and Data.hasAutoTile(cell):
                            if cell not in autoTileIndexByKey:
                                autoTileIndexByKey[cell] = len(autoTilePool)
                                autoTilePool.append(Data.getAutoTile(cell))
                            row.append(autoTileIndexByKey[cell])
                        elif isinstance(cell, int) and 0 <= cell < len(autoTilePool):
                            row.append(cell)
                        else:
                            row.append(None)
                    autoTileGrid.append(row)
            else:
                for _ in range(height):
                    autoTileGrid.append([None] * width)
            tileLayerData = TileLayerData(
                name,
                layerTileset,
                tiles,
                autoTileGrid,
                autoTilePool,
                [key for key, _ in sorted(autoTileIndexByKey.items(), key=lambda item: item[1])],
                str(layerData.get("shaderPath", "") or ""),
            )
            autoTileTextures = [Manager.loadAutotile(entry.fileName) for entry in autoTilePool]
            autoTileFrameCounts = []
            for texture in autoTileTextures:
                size = texture.getSize()
                cellSize = Engine.CellSize
                frames = size.x // (3 * cellSize) if cellSize > 0 else 1
                if frames < 1:
                    frames = 1
                autoTileFrameCounts.append(frames)
            layer = TileLayer(
                tileLayerData,
                Manager.loadTileset(tileLayerData.layerTileset.fileName),
                autoTileTextures,
                autoTileFrameCounts,
            )
            mapLayers.append(layer)
        return Tilemap(mapLayers)

    def generateGameMap(
        self,
        data: Dict[str, Any],
        camera: Optional[Camera] = None,
        emitCreateEvents: bool = True,
    ) -> GameMap:
        r"""\brief Generate a game map from serialised map data.

        - \param data Map data.
        - \param camera Optional camera.
        - \param emitCreateEvents Whether actor/component create events should run.
        - \return Generated game map.
        """
        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        tilemap = self.generateTilemap(layers, width, height)
        ambientLight = data.get("ambientLight", [255, 255, 255, 255])
        lights = data.get("lights", [])
        actors = data.get("actors", [])
        classVarChanges = data.get("BPClassVarChanged")
        result = GameMap(mapName, tilemap, camera)
        result.setAmbientLight(Color(*ambientLight))
        for lightData in lights:
            result.addLight(Light.fromDict(lightData))
        for layerName, actorDatas in actors.items():
            for actorData in actorDatas:
                actorChanges = None
                if isinstance(actorData, dict) and isinstance(classVarChanges, dict):
                    tag = str(actorData.get("tag", ""))
                    changes = classVarChanges.get(tag)
                    if isinstance(changes, dict):
                        actorChanges = changes
                actor = Data.genActorFromData(actorData, layerName, actorChanges)
                if actor is None:
                    continue
                result.spawnActor(actor, layerName, False)
        if emitCreateEvents:
            result.initialiseActorsAndComponents()
        return result

    def buildFloorMapPreview(
        self,
        inst: GameInstance,
        currentMap: Optional[str],
        mapKey: str,
        telepoint: Tuple[int, int],
        previewSize: int,
        previewScale: float,
        showTelepointMarker: bool = False,
    ) -> Optional[Texture]:
        r"""\brief Build a floor teleporter preview texture.

        - \param inst Current game instance.
        - \param currentMap Current map path used to resolve extension-less map keys.
        - \param mapKey Region map key.
        - \param telepoint Telepoint tile position.
        - \param previewSize Preview texture size in pixels.
        - \param previewScale Preview map scale.
        - \param showTelepointMarker Whether to draw the selected telepoint marker.
        - \return Preview texture, or None on failure.
        """
        try:
            mapPath, mapData = self.loadMapData(mapKey, currentMap)
            gameMap = self.generateGameMap(mapData, emitCreateEvents=False)
            gameMap.applyTerrainDestructions(inst.getTerrainDestructions(mapPath))
            gameMap.applyAddedActors(inst.getAddedActors(mapPath), emitCreateEvents=False)
            gameMap.applyActorPositions(inst.getActorPositions(mapPath))
            gameMap.removeActorsByTags(inst.getDestroyedActors(mapPath))
        except Exception:
            return None
        if previewScale <= 0.0:
            previewScale = 1.0
        target = RenderTexture(Vector2u(previewSize, previewSize))
        target.clear(Color.Transparent)
        viewSize = Vector2f(float(previewSize) / previewScale, float(previewSize) / previewScale)
        mapPixelSize = Vector2f(
            float(mapData["width"] * Engine.CellSize),
            float(mapData["height"] * Engine.CellSize),
        )
        centre = Vector2f(
            viewSize.x / 2.0 if mapPixelSize.x >= viewSize.x else mapPixelSize.x / 2.0,
            viewSize.y / 2.0 if mapPixelSize.y >= viewSize.y else mapPixelSize.y / 2.0,
        )
        cellSize = Engine.CellSize
        telepointCentre = Vector2f(
            (float(telepoint[0]) + 0.5) * cellSize,
            (float(telepoint[1]) + 0.5) * cellSize,
        )
        halfView = viewSize / 2.0
        if (
            telepointCentre.x < centre.x - halfView.x
            or telepointCentre.x > centre.x + halfView.x
            or telepointCentre.y < centre.y - halfView.y
            or telepointCentre.y > centre.y + halfView.y
        ):
            centre = telepointCentre
        target.setView(View(centre, viewSize))
        states = Render.CanvasRenderStates()
        gameMap.drawMapContent(target, states)
        if showTelepointMarker:
            marker = RectangleShape(Vector2f(float(cellSize), float(cellSize)))
            marker.setPosition(Vector2f(float(telepoint[0] * cellSize), float(telepoint[1] * cellSize)))
            marker.setFillColor(Color(0, 255, 0, 64))
            marker.setOutlineColor(Color(0, 255, 0, 255))
            marker.setOutlineThickness(2.0)
            target.draw(marker, states)
        target.display()
        texture = Texture(target.getTexture().copyToImage())
        texture.setSmooth(False)
        return texture

    def getFloorTelepointTag(
        self,
        currentMap: Optional[str],
        mapKey: str,
        telepoint: Tuple[int, int],
    ) -> Optional[str]:
        r"""\brief Get the teleporter actor tag for a floor telepoint.

        - \param currentMap Current map path used to resolve extension-less map keys.
        - \param mapKey Region map key.
        - \param telepoint Telepoint tile position.
        - \return Teleporter actor tag, or None.
        """
        try:
            _, mapData = self.loadMapData(mapKey, currentMap)
        except Exception:
            return None
        targetPosition = [telepoint[0], telepoint[1]]
        for actorDatas in mapData.get("actors", {}).values():
            for actorData in actorDatas:
                if actorData.get("position") != targetPosition:
                    continue
                bp = str(actorData.get("bp", ""))
                if not bp.startswith("Data.Blueprints.Teleportations"):
                    continue
                return str(actorData.get("tag", ""))
        return None

    def resolveRegionMapPath(self, mapKey: str, currentMap: Optional[str] = None) -> str:
        r"""\brief Resolve a region map key to a map data path.

        - \param mapKey Region map key or map file path.
        - \param currentMap Current map path used to inherit extension.
        - \return Resolved map file path.
        """
        return self.resolveMapPath(mapKey, currentMap)
