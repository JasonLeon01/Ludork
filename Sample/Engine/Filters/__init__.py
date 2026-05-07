# -*- encoding: utf-8 -*-

r"""Audio filter package.

Provides sound effect processors and filters for the Ludork sample engine.

- EffectProcessor  Process sound effects on audio sources
- SoundFilter      Filter for sound effects
- MusicFilter      Filter for music tracks
"""

from .Sound import EffectProcessor, SoundFilter, MusicFilter, echoEffect, distortionEffect, underwaterEffect

__all__ = ["EffectProcessor", "SoundFilter", "MusicFilter", "echoEffect", "distortionEffect", "underwaterEffect"]
