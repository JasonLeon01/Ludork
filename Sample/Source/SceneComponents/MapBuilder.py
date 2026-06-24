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


class SceneMapBuilder:
    r"""\brief Build map runtime objects and floor-map previews for SceneMap."""

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
        result = GameMap(mapName, tilemap, camera)
        result.setAmbientLight(Color(*ambientLight))
        for lightData in lights:
            result.addLight(Light.fromDict(lightData))
        for layerName, actorDatas in actors.items():
            for actorData in actorDatas:
                actor = Data.genActorFromData(actorData, layerName)
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
        mapPath = self.resolveRegionMapPath(mapKey, currentMap)
        try:
            mapData = File.loadData(os.path.join("./Data/Maps", mapPath))
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
        mapPath = self.resolveRegionMapPath(mapKey, currentMap)
        try:
            mapData = File.loadData(os.path.join("./Data/Maps", mapPath))
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
        if os.path.splitext(mapKey)[1]:
            return mapKey
        candidates: List[str] = []
        currentMap = currentMap or System.getStartMap()
        currentExt = os.path.splitext(currentMap)[1] if currentMap else ""
        if currentExt:
            candidates.append(f"{mapKey}{currentExt}")
        candidates.extend([f"{mapKey}.dat", f"{mapKey}.json"])
        for candidate in candidates:
            if os.path.exists(os.path.join("./Data/Maps", candidate)):
                return candidate
        return candidates[0] if candidates else mapKey
