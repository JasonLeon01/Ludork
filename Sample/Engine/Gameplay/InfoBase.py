# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict, List, Optional, Protocol
from ..BPBase import BPBase
from ..NodeGraph import Graph


class InfoDataProvider(Protocol):
    r"""\brief Data provider used by `InfoBase.initInfo`."""

    def getGeneralData(self, name: str) -> Dict[str, Any]:
        r"""\brief Load a GeneralData table by type name."""
        ...

    def genGraphFromData(
        self,
        data: Dict[str, Any],
        parent: Optional[object] = None,
        parentClass: Optional[type] = None,
    ) -> Graph:
        r"""\brief Build a blueprint graph from serialised graph data."""
        ...


@Meta(
    VariableDisplayNames={
        "ID": 'LOC("INFO_VAR_ID")',
    },
    VariableDisplayDescs={
        "ID": 'LOC("INFO_VAR_ID_DESC")',
    },
)
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
    _infoGraph: Optional[Graph] = None
    _graph: Optional[Graph] = None

    ID: str = ""  #: Unique identifier for this info object

    def initInfo(self, dataProvider: InfoDataProvider) -> None:
        r"""Load attributes from GeneralData onto self.

        Also builds the info-layer Graph if _graph data exists in the member.

        - \param dataProvider  Optional module/object providing getGeneralData(name).
        """
        if not self._infoType:
            return

        datas = dataProvider.getGeneralData(self._infoType)
        members = datas.get("members", {})
        params = datas.get("params", {})
        member_data = members.get(self.ID, {})
        if member_data:
            self.ApplyGeneralData(self, member_data, params)
            graphData = member_data.get("_graph")
            if graphData:
                self._infoGraph = dataProvider.genGraphFromData(graphData, self, type(self))

    def setInfoGraph(self, graph: Optional[Graph]) -> None:
        r"""Set the info-layer blueprint Graph.

        This graph handles data-level events (onUse, onEquip, etc).

        - \param graph  The blueprint `Graph` instance to assign
        """
        self._infoGraph = graph

    def getInfoGraph(self) -> Optional[Graph]:
        r"""\brief Get the info-layer blueprint graph.

        - \return The info-layer graph, or None if unset.
        """
        return self._infoGraph

    def hasInfoGraph(self) -> bool:
        r"""\brief Check whether this info object has an info-layer graph.

        - \return True if an info-layer graph is assigned.
        """
        return self._infoGraph is not None

    def triggerEvent(self, eventName: str, **kwargs: Any) -> None:
        r"""Convenience method to trigger a blueprint event.

        For standalone Info objects, executes _infoGraph directly.
        For Actor-bridged objects, delegates to BPBase.BlueprintEvent.

        - \param eventName  Name of the event to trigger
        - \param kwargs     Additional keyword arguments passed to the event
        """
        from .Actors.Base import _ActorBase

        if isinstance(self, _ActorBase) and self.getGraph() is not None:
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
