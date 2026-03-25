# -*- encoding: utf-8 -*-

from dataclasses import fields
import os
from typing import Any
from Engine import Manager, Filters, playVideo

SoundFilter = Filters.SoundFilter()
MusicFilter = Filters.MusicFilter()


@Meta(
    DisplayName='LOC("EDIT_SOUND_FILTER")',
    DisplayDesc='LOC("EDIT_SOUND_FILTER_DESC")',
    DropBox={"attr": [field.name for field in fields(Filters.SoundFilter)]},
)
@ExecSplit(default=(None,))
def EditSoundFilter(attr: str, value: Any) -> None:
    setattr(SoundFilter, attr, value)


@Meta(
    DisplayName='LOC("EDIT_MUSIC_FILTER")',
    DisplayDesc='LOC("EDIT_MUSIC_FILTER_DESC")',
    DropBox={"attr": [field.name for field in fields(Filters.MusicFilter)]},
)
@ExecSplit(default=(None,))
def EditMusicFilter(attr: str, value: Any) -> None:
    setattr(MusicFilter, attr, value)


@Meta(DisplayName='LOC("PLAY_SOUND")', DisplayDesc='LOC("PLAY_SOUND_DESC")')
@ExecSplit(default=(None,))
def PlaySound(soundFileName: str, applyFilter: bool) -> None:
    global SoundFilter
    if applyFilter:
        Manager.playSE(soundFileName, SoundFilter)
    else:
        Manager.playSE(soundFileName)


@Meta(DisplayName='LOC("PLAY_MUSIC")', DisplayDesc='LOC("PLAY_MUSIC_DESC")')
@ExecSplit(default=(None,))
def PlayMusic(musicFileName: str, applyFilter: bool) -> None:
    global MusicFilter
    if applyFilter:
        Manager.playMusic("bgm", musicFileName, MusicFilter)
    else:
        Manager.playMusic("bgm", musicFileName)


@Meta(DisplayName='LOC("PLAY_VIDEO")', DisplayDesc='LOC("PLAY_VIDEO_DESC")')
@ExecSplit(default=(None,))
def PlayVideo(videoFileName: str, mute: bool, skipable: bool) -> None:
    videoPath = os.path.join(os.getcwd(), "Assets", "Videos", videoFileName)
    playVideo(videoPath, mute, skipable)
