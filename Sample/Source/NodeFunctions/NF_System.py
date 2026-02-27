# -*- encoding: utf-8 -*-

from dataclasses import fields
import os
from typing import Any
from Engine import ExecSplit, DropBox, Manager, Filters, playVideo

SoundFilter = Filters.SoundFilter()
MusicFilter = Filters.MusicFilter()


@ExecSplit(default=(None,))
@DropBox(attr=[field.name for field in fields(Filters.SoundFilter)])
def EditSoundFilter(attr: str, value: Any) -> None:
    setattr(SoundFilter, attr, value)


@ExecSplit(default=(None,))
@DropBox(attr=[field.name for field in fields(Filters.MusicFilter)])
def EditMusicFilter(attr: str, value: Any) -> None:
    setattr(MusicFilter, attr, value)


@ExecSplit(default=(None,))
def PlaySound(soundFileName: str, applyFilter: bool) -> None:
    global SoundFilter
    if applyFilter:
        Manager.playSE(soundFileName, SoundFilter)
    else:
        Manager.playSE(soundFileName)


@ExecSplit(default=(None,))
def PlayMusic(musicFileName: str, applyFilter: bool) -> None:
    global MusicFilter
    if applyFilter:
        Manager.playMusic("bgm", musicFileName, MusicFilter)
    else:
        Manager.playMusic("bgm", musicFileName)


@ExecSplit(default=(None,))
def PlayVideo(videoFileName: str, mute: bool, skipable: bool) -> None:
    videoPath = os.path.join(os.getcwd(), "Assets", "Videos", videoFileName)
    playVideo(videoPath, mute, skipable)
