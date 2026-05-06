# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List
from ..BPBase import BPBase


class InfoBase(BPBase):
    """
    Pure data-layer base class, independent of Actor.
    Separates "data + blueprint event logic" from "scene entity".

    Subclasses should set _infoType to the corresponding GeneralData type name,
    and mark overridable events with @RegisterEvent.

    Usage:
        - Inventory/UI logic can instantiate Info subclasses directly
        - Scene Actors bridge via multiple inheritance (Actor, XxxInfo)
    """

    _infoType: str = ""
    _infoGraph = None
    _graph = None

    ID: str = ""

    def initInfo(self, dataProvider=None) -> None:
        """
        Load attributes from GeneralData onto self.
        Also builds the info-layer Graph if _graph data exists in the member.
        dataProvider: optional module/object providing getGeneralData(name).
                      Falls back to Source.Data if None.
        """
        if not self._infoType:
            return

        if dataProvider is None:
            from Source import Data as dataProvider  # type: ignore

        datas = dataProvider.getGeneralData(self._infoType)
        members = datas.get("members", {})
        params = datas.get("params", {})
        member_data = members.get(self.ID, {})
        if member_data:
            self.ApplyGeneralData(self, member_data, params)
            graphData = member_data.get("_graph")
            if graphData:
                self._infoGraph = dataProvider.genGraphFromData(graphData, self, type(self))

    def setInfoGraph(self, graph) -> None:
        """
        Set the info-layer blueprint Graph.
        This graph handles data-level events (onUse, onEquip, etc).
        """
        self._infoGraph = graph

    def triggerEvent(self, eventName: str, **kwargs) -> None:
        """
        Convenience method to trigger a blueprint event.
        For standalone Info objects, executes _infoGraph directly.
        For Actor-bridged objects, delegates to BPBase.BlueprintEvent.
        """
        if hasattr(self, "_graph") and self._graph is not None:
            self.BlueprintEvent(self, type(self), eventName, kwargs if kwargs else None)
        else:
            self._tryExecuteInfoGraph(self, eventName, kwargs if kwargs else {})

    @classmethod
    def getRegisteredEvents(cls) -> List[str]:
        """Return a list of all @RegisterEvent-decorated event names on this class."""
        events = []
        for name in dir(cls):
            if name.startswith("_"):
                continue
            attr = getattr(cls, name, None)
            if callable(attr) and getattr(attr, "_eventSignature", False):
                events.append(name)
        return events

    @classmethod
    def getInfoType(cls) -> str:
        """Return the linked GeneralData type name."""
        return cls._infoType
