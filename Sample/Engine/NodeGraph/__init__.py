# -*- encoding: utf-8 -*-

from .Node import DataNode, Node
from .Graph import Graph
from .ClassDict import ClassDict
from .LatentManager import LatentManager

latentManager: LatentManager = None


def initLatent() -> None:
    global latentManager
    latentManager = LatentManager()
