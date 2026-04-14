# -*- encoding: utf-8 -*-

from . import Manager
from . import Utils
from .Animation import Animation
from .Video import Video
from .System import System
from .GameMap import Light, GameMap
from .Camera import Camera
from .SceneBase import SceneBase
from .UIManager import UIManager


def playVideo(videoPath: str, mute: bool = False, skipable: bool = False):
    video = Video(videoPath, mute, skipable)
    video.play()
