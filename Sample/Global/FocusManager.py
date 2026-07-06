# -*- encoding: utf-8 -*-
r"""\brief Keyboard focus manager for runtime UI navigation."""

from __future__ import annotations
from enum import Enum
from typing import Dict, List, Optional, Set, Union
from Engine.UI.Base import ControlBase, FunctionalBase
from Engine.UI.Base.FocusableMixin import Direction


class FocusTransition(Enum):
    r"""\brief Strategy used when moving from one focus group to a neighbour."""

    DIRECTIONAL = "directional"
    EXPLICIT = "explicit"


class FocusNeighbor:
    r"""\brief Directional neighbour metadata for a focus group."""

    def __init__(
        self,
        group: "FocusGroup",
        transition: FocusTransition = FocusTransition.DIRECTIONAL,
    ) -> None:
        r"""\brief Construct a focus neighbour edge.

        - \param group Target focus group.
        - \param transition Navigation strategy for this edge.
        """
        self.group = group
        self.transition = transition


class FocusGroup:
    r"""\brief Group of controls that share a directional focus neighbourhood."""

    def __init__(
        self,
        name: str,
        items: Optional[List[FunctionalBase]] = None,
        activeOwner: Optional[FunctionalBase] = None,
    ) -> None:
        r"""\brief Construct a focus group.

        - \param name Debug name for the group.
        - \param items Initial focusable controls in the group.
        - \param activeOwner Control whose active state gates group entry.
        """
        self.name = name
        self.activeOwner = activeOwner
        self._items: List[FunctionalBase] = []
        self._neighborMap: Dict[Direction, FocusNeighbor] = {}
        self._lastFocusedElement: Optional[FunctionalBase] = None
        if items is not None:
            for item in items:
                self.addItem(item)

    def addItem(self, item: FunctionalBase) -> None:
        r"""\brief Add a control to this focus group.

        - \param item Control to add.
        """
        if item in self._items:
            return
        self._items.append(item)
        item.setFocusGroup(self)

    def removeItem(self, item: FunctionalBase) -> None:
        r"""\brief Remove a control from this focus group.

        - \param item Control to remove.
        """
        if item not in self._items:
            return
        self._items.remove(item)
        if item.getFocusGroup() is self:
            item.setFocusGroup(None)
        if self._lastFocusedElement is item:
            self._lastFocusedElement = None

    def getItems(self) -> List[FunctionalBase]:
        r"""\brief Return controls registered in this group.

        - \return Registered focusable controls.
        """
        return self._items[:]

    def setNeighbor(
        self,
        direction: Direction,
        neighbor: Union["FocusGroup", FocusNeighbor],
        transition: FocusTransition = FocusTransition.DIRECTIONAL,
    ) -> None:
        r"""\brief Set the neighbouring group for a direction.

        - \param direction Navigation direction.
        - \param neighbor Target group or neighbour metadata.
        - \param transition Navigation strategy when `neighbor` is a group.
        """
        if isinstance(neighbor, FocusNeighbor):
            self._neighborMap[direction] = neighbor
        else:
            self._neighborMap[direction] = FocusNeighbor(neighbor, transition)

    def getNeighbor(self, direction: Direction) -> Optional[FocusNeighbor]:
        r"""\brief Return the neighbour for a direction.

        - \param direction Navigation direction.
        - \return Focus neighbour or None.
        """
        return self._neighborMap.get(direction)

    def canEnter(self) -> bool:
        r"""\brief Return whether focus may enter this group.

        - \return True if the group has an active focusable target.
        """
        if self.activeOwner is not None and not self._isOwnerAvailable(self.activeOwner):
            return False
        return self._findInitialFocusLocal() is not None

    def findInitialFocus(self) -> Optional[FunctionalBase]:
        r"""\brief Return the focus target to use when entering this group.

        - \return Focusable control or None.
        """
        if self.activeOwner is not None and not self._isOwnerAvailable(self.activeOwner):
            return None
        return self._findInitialFocusLocal()

    def rememberFocus(self, element: FunctionalBase) -> None:
        r"""\brief Store the last focused element in this group.

        - \param element Focused element.
        """
        if element in self._items:
            self._lastFocusedElement = element

    def moveWithin(self, current: FunctionalBase, direction: Direction) -> Optional[FunctionalBase]:
        r"""\brief Move within the group before trying neighbour groups.

        - \param current Currently focused element.
        - \param direction Navigation direction.
        - \return New focus target or None when the group does not handle it.
        """
        return None

    def _findInitialFocusLocal(self) -> Optional[FunctionalBase]:
        if self._lastFocusedElement is not None and self._isLocallyFocusable(self._lastFocusedElement):
            return self._lastFocusedElement
        for item in self._items:
            if self._isLocallyFocusable(item):
                return item
        return None

    @staticmethod
    def _isLocallyFocusable(element: FunctionalBase) -> bool:
        return element.canReceiveFocus()

    @staticmethod
    def _isOwnerAvailable(element: FunctionalBase) -> bool:
        if not element.getActive():
            return False
        if isinstance(element, ControlBase) and not element.getVisible():
            return False
        return True


class FocusManager:
    r"""\brief Scene-level keyboard focus manager."""

    def __init__(self) -> None:
        r"""\brief Construct a focus manager with navigation disabled."""
        self._focusedElement: Optional[FunctionalBase] = None
        self._cursorFocusElement: Optional[FunctionalBase] = None
        self._focusGroups: List[FocusGroup] = []
        self._autoFocusGroups: Dict[FunctionalBase, FocusGroup] = {}
        self._navigationEnabled: bool = False

    def setNavigationEnabled(self, enabled: bool) -> None:
        r"""\brief Enable or disable focus-based keyboard routing.

        - \param enabled True to route keyboard events only to the focused control.
        """
        self._navigationEnabled = enabled
        if not enabled:
            self.clearFocus()

    def getNavigationEnabled(self) -> bool:
        r"""\brief Return whether focus routing is enabled.

        - \return True when focus routing is enabled.
        """
        return self._navigationEnabled

    def isRoutingKeyboard(self) -> bool:
        r"""\brief Return whether keyboard input should be focus-routed.

        - \return True when focus routing is enabled.
        """
        return self._navigationEnabled

    def registerElement(self, element: FunctionalBase) -> None:
        r"""\brief Register a top-level focusable element for automatic focus.

        - \param element UI element to register.
        """
        for group in self._focusGroups:
            if element in group.getItems():
                return
        if element in self._autoFocusGroups:
            return
        name = element.getName() if isinstance(element, ControlBase) else ""
        group = FocusGroup(f"auto:{name or id(element)}", [element], element)
        self._autoFocusGroups[element] = group

    def unregisterElement(self, element: FunctionalBase) -> None:
        r"""\brief Unregister a top-level focusable element.

        - \param element UI element to unregister.
        """
        group = self._autoFocusGroups.pop(element, None)
        if group is not None:
            group.removeItem(element)
        if self._focusedElement is element:
            self.clearFocus()
        if self._cursorFocusElement is element:
            self._cursorFocusElement = None

    def registerFocusGroup(self, group: FocusGroup) -> None:
        r"""\brief Register an explicit focus group.

        - \param group Focus group to register.
        """
        if group not in self._focusGroups:
            self._focusGroups.append(group)

    def unregisterFocusGroup(self, group: FocusGroup) -> None:
        r"""\brief Unregister an explicit focus group.

        - \param group Focus group to unregister.
        """
        if group in self._focusGroups:
            self._focusGroups.remove(group)
        if self._focusedElement is not None and self._findGroupForElement(self._focusedElement) is group:
            self.clearFocus()

    def getFocus(self) -> Optional[FunctionalBase]:
        r"""\brief Return the currently focused element.

        - \return Focused element or None.
        """
        return self._focusedElement

    def setFocus(self, element: FunctionalBase) -> bool:
        r"""\brief Move keyboard focus to an element.

        - \param element Target element.
        - \return True if focus moved to the target.
        """
        if not self._isFocusable(element):
            return False
        if self._focusedElement is element:
            self._cursorFocusElement = element
            return True
        self.clearFocus()
        self._focusedElement = element
        self._cursorFocusElement = element
        element.setFocused(True)
        group = self._findGroupForElement(element)
        if group is not None:
            group.rememberFocus(element)
        return True

    def clearFocus(self) -> None:
        r"""\brief Clear the current keyboard focus."""
        if self._focusedElement is None:
            return
        focusedElement = self._focusedElement
        self._focusedElement = None
        if self._cursorFocusElement is focusedElement:
            self._cursorFocusElement = None
        focusedElement.setFocused(False)

    def prepareFrame(self) -> None:
        r"""\brief Validate focus before UI update dispatch for the frame."""
        if not self._navigationEnabled:
            return
        if self._focusedElement is not None and self._isFocusable(self._focusedElement):
            return
        self.clearFocus()
        nextFocus = self._findDefaultFocus()
        if nextFocus is not None:
            self.setFocus(nextFocus)

    def shouldDispatchKeyboardTo(self, element: FunctionalBase) -> bool:
        r"""\brief Return whether an element may receive keyboard callbacks.

        - \param element UI element being updated.
        - \return True if keyboard callbacks should be dispatched to it.
        """
        if not self._navigationEnabled:
            return True
        return self._focusedElement is element and self._isFocusable(element)

    def isFocused(self, element: FunctionalBase) -> bool:
        r"""\brief Return whether an element is focused.

        - \param element UI element to test.
        - \return True if this element owns focus.
        """
        return self._focusedElement is element

    def isCursorFocusOwner(self, element: FunctionalBase) -> bool:
        r"""\brief Return whether an element owns the selection cursor.

        - \param element UI element to test.
        - \return True if this element owns the keyboard selection cursor.
        """
        if not self._navigationEnabled:
            return False
        return self._cursorFocusElement is element and self._isFocusable(element)

    def requestDirectionalMove(self, element: FunctionalBase, direction: Direction) -> bool:
        r"""\brief Request a directional focus move from an element.

        - \param element Source element.
        - \param direction Navigation direction.
        - \return True if focus moved.
        """
        if not self._navigationEnabled:
            return False
        if self._focusedElement is not element:
            return False
        return self.moveFocus(direction, element)

    def moveFocus(self, direction: Direction, source: Optional[FunctionalBase] = None) -> bool:
        r"""\brief Move focus in a direction.

        - \param direction Navigation direction.
        - \param source Optional source element; current focus is used when omitted.
        - \return True if focus moved.
        """
        if not self._navigationEnabled:
            return False
        current = source if source is not None else self._focusedElement
        if current is None:
            return False
        group = self._findGroupForElement(current)
        if group is None:
            return False
        withinTarget = group.moveWithin(current, direction)
        if withinTarget is not None:
            return self.setFocus(withinTarget)
        neighborTarget = self._findDirectionalTarget(group, direction)
        if neighborTarget is None:
            return False
        return self.setFocus(neighborTarget)

    def activateGroup(self, group: FocusGroup) -> bool:
        r"""\brief Focus the default element in a group.

        - \param group Focus group to activate.
        - \return True if focus moved into the group.
        """
        target = group.findInitialFocus()
        if target is None:
            return False
        return self.setFocus(target)

    def _findDefaultFocus(self) -> Optional[FunctionalBase]:
        for group in self._allGroups():
            target = group.findInitialFocus()
            if target is not None:
                return target
        return None

    def _findDirectionalTarget(self, group: FocusGroup, direction: Direction) -> Optional[FunctionalBase]:
        currentGroup = group
        visited: Set[int] = {id(currentGroup)}
        while True:
            neighbor = currentGroup.getNeighbor(direction)
            if neighbor is None or neighbor.transition != FocusTransition.DIRECTIONAL:
                return None
            nextGroup = neighbor.group
            if id(nextGroup) in visited:
                return None
            visited.add(id(nextGroup))
            target = nextGroup.findInitialFocus()
            if target is not None:
                return target
            currentGroup = nextGroup

    def _isFocusable(self, element: FunctionalBase) -> bool:
        if not element.canReceiveFocus():
            return False
        group = self._findGroupForElement(element)
        if group is not None and not group.canEnter():
            return False
        return True

    def _findGroupForElement(self, element: FunctionalBase) -> Optional[FocusGroup]:
        for group in self._focusGroups:
            if element in group.getItems():
                return group
        explicitGroup = element.getFocusGroup()
        if isinstance(explicitGroup, FocusGroup) and explicitGroup in self._focusGroups:
            return explicitGroup
        return self._autoFocusGroups.get(element)

    def _allGroups(self) -> List[FocusGroup]:
        return self._focusGroups + list(self._autoFocusGroups.values())


__all__ = ["FocusManager", "FocusGroup", "FocusNeighbor", "FocusTransition"]
