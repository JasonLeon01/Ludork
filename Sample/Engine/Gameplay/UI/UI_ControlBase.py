# -*- encoding: utf-8 -*-


class Base:
    def __init__(self) -> None:
        self._visible: bool = True

    def getVisible(self) -> bool:
        return self._visible

    def setVisible(self, visible: bool) -> None:
        self._visible = visible
