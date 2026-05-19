# -*- encoding: utf-8 -*-
r"""
\brief Global package.

Provides engine-level systems shared across all game scenes:

- Animation       Animation sprite with sound sync
- Camera          Viewport tracking and screen-space transforms
- Components      Reusable gameplay components
- CustomParticles Custom particle controllers
- Weather          RMXP-style screen weather (shader / iOS particles)
- Fog              RMXP-style full-screen map fog (shader / iOS sprite scroll)
- GameMap         Tile map management with lighting and pathfinding
- Manager         Resource manager facade (audio, fonts, textures, shaders)
- SceneBase       Abstract base class for scenes
- System          Global system: window, scene transitions, rendering
- UIManager       UI canvas management and event dispatch
- Utils           Rendering utility functions
- Video           Video playback with frame-by-frame decoding
"""

from . import Manager
from . import Utils
from .Animation import Animation
from .Video import Video
from .System import System
from .GameMap import Light, GameMap
from .Camera import Camera
from .SceneBase import SceneBase
from .UIManager import UIManager
from .Weather import WeatherController, WeatherType
from .Fog import FogController


def playVideo(videoPath: str, mute: bool = False, skipable: bool = False) -> None:
    video = Video(videoPath, mute, skipable)
    video.play()
