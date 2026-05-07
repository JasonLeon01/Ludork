# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List
from ..BPBase import BPBase


class InfoBase(BPBase):
    r"""Pure data-layer base class, independent of Actor.

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

    ID: str = ""  #: Unique identifier for this info object

    def initInfo(self, dataProvider) -> None:
        r"""Load attributes from GeneralData onto self.

        Also builds the info-layer Graph if _graph data exists in the member.

        - \param dataProvider  Optional module/object providing getGeneralData(name).
        """
        if not self._infoType:
            return

        datas = dataProvider.getGeneralData(self._infoType)  # type: ignore
        members = datas.get("members", {})
        params = datas.get("params", {})
        member_data = members.get(self.ID, {})
        if member_data:
            self.ApplyGeneralData(self, member_data, params)
            graphData = member_data.get("_graph")
            if graphData:
                self._infoGraph = dataProvider.genGraphFromData(graphData, self, type(self))  # type: ignore

    def setInfoGraph(self, graph) -> None:
        r"""Set the info-layer blueprint Graph.

        This graph handles data-level events (onUse, onEquip, etc).

        - \param graph  The blueprint `Graph` instance to assign
        """
        self._infoGraph = graph

    def triggerEvent(self, eventName: str, **kwargs) -> None:
        r"""Convenience method to trigger a blueprint event.

        For standalone Info objects, executes _infoGraph directly.
        For Actor-bridged objects, delegates to BPBase.BlueprintEvent.

        - \param eventName  Name of the event to trigger
        - \param kwargs     Additional keyword arguments passed to the event
        """
        if hasattr(self, "_graph") and self._graph is not None:
            self.BlueprintEvent(self, type(self), eventName, kwargs if kwargs else None)
        else:
            self._tryExecuteInfoGraph(self, eventName, kwargs if kwargs else {})

    @classmethod
    def getRegisteredEvents(cls) -> List[str]:
        r"""Return a list of all @RegisterEvent-decorated event names on this class.

        - \return  List of event name strings
        """
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
        r"""Return the linked GeneralData type name.

        - \return  The `_infoType` string, or empty string if not set
        """
        return cls._infoType
