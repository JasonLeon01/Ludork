# -*- encoding: utf-8 -*-

r"""
\brief State package.

Provides finite-state machine support for the Ludork sample engine.

- StateInfo  State metadata descriptor
- State      Base state class
"""

from .State import StateInfo, State

__all__ = ["StateInfo", "State"]
