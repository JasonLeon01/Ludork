# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass, asdict, field, fields
from typing import Any, Dict
from .Material import Material


@dataclass
class AutoTile:
    r"""AutoTile dataclass.

    Defines an auto-tiling tile entry. Unlike `Tileset`, an `AutoTile`
    represents a single logical tile whose appearance is derived from a
    source image laid out in the standard 3-columns x 4-rows mini-tile
    pattern. The image width may be a multiple of 3 cells when the
    auto-tile is animated (one mini-pattern per animation frame).

    Each `AutoTile` carries a single passability flag and a single
    material because the whole entry behaves as one tile in gameplay.
    """

    name: str  #: AutoTile name
    fileName: str  #: Texture image file name (relative to Assets/Autotiles)
    passable: bool = True  #: Whether the auto-tile can be walked on
    material: Material = field(default_factory=Material)  #: Material applied to the whole auto-tile

    def asDict(self) -> Dict[str, Any]:
        r"""Serialize the auto-tile to a dictionary.

        - \return  Dictionary containing all auto-tile fields
        """
        return asdict(self)

    @staticmethod
    def fromData(data: Dict[str, Any]) -> AutoTile:
        r"""Create an `AutoTile` from a raw data dictionary.

        - \param data  Raw dictionary, e.g. loaded from JSON or .dat
        - \return      The created `AutoTile` instance
        """
        validKeys = {f.name for f in fields(AutoTile)}
        payload = {k: v for k, v in data.items() if k in validKeys}
        materialData = payload.get("material")
        if isinstance(materialData, dict):
            materialKeys = {f.name for f in fields(Material)}
            payload["material"] = Material(**{k: v for k, v in materialData.items() if k in materialKeys})
        elif materialData is None:
            payload["material"] = Material()
        return AutoTile(**payload)
