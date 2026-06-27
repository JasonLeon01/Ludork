# -*- encoding: utf-8 -*-

from typing import List, Optional, Tuple
from PyQt5 import QtGui


def QImageToRgbaTuple(image: QtGui.QImage) -> Tuple[bytes, int, int, int]:
    rgba = image.convertToFormat(QtGui.QImage.Format_RGBA8888)
    bits = rgba.bits()
    if not bits:
        raise RuntimeError("QImage has no pixel data")
    stride = rgba.bytesPerLine()
    size = rgba.height() * stride
    return bytes(bits.asstring(size)), rgba.width(), rgba.height(), stride


def RgbaBytesToQImage(data: bytes, width: int, height: int) -> QtGui.QImage:
    image = QtGui.QImage(data, width, height, QtGui.QImage.Format_RGBA8888)
    return image.copy()


def GridToStringGrid(autoTiles: List[List[Optional[str]]]) -> List[List[str]]:
    out: List[List[str]] = []
    for row in autoTiles:
        if not isinstance(row, list):
            out.append([])
            continue
        out.append([key if isinstance(key, str) and key else "" for key in row])
    return out
