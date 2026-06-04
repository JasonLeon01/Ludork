# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any


class ComponentBase:
    r"""
    \brief Base class for attachable gameplay components.

    Components are attached to game objects (actors) and provide
    reusable functionality through lifecycle callbacks.
    """

    def __init__(self, parent: Any) -> None:
        r"""
        \brief Initialize the component with a parent object.

        - parent: The parent game object that this component is attached to.
        """
        self._parent: Any = parent

    def onTick(self) -> None:
        r"""
        \brief Called every frame to update component logic.

        Override this method to implement per-frame logic.
        """
        return

    def onLateTick(self) -> None:
        r"""
        \brief Called after onTick for late-update logic.

        Override this method to implement logic that should run
        after the main tick update.
        """
        return

    def onFixedTick(self) -> None:
        r"""
        \brief Called at fixed timestep for physics-like updates.

        Override this method to implement logic that should run
        at a fixed timestep.
        """
        return

    def onRender(self, camera: Any) -> None:
        r"""
        \brief Called to render component visuals.

        - camera: The camera to use for rendering.
        """
        return
