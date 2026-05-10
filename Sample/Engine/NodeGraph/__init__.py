# -*- encoding: utf-8 -*-

r"""
\brief Node graph package.

Provides node-based graph data structures and latent execution management
for the Ludork sample engine.

- DataNode       Base node with serialisable data
- Node           Graph node with connections
- Graph          Node graph container
- ClassDict      Dictionary mapping class names to classes
- LatentManager  Manages latent (delayed) execution
"""

from .Node import DataNode, Node
from .Graph import Graph
from .ClassDict import ClassDict
from .LatentManager import LatentManager

latentManager: LatentManager


def initLatent() -> None:
    r"""
    \brief init Latent.
    """

    global latentManager
    latentManager = LatentManager()


__all__ = ["DataNode", "Node", "Graph", "ClassDict", "LatentManager", "initLatent"]
