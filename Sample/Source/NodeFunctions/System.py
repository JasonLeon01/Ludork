# -*- encoding: utf-8 -*-
r"""\brief Blueprint system nodes: scene transitions, save/load, and game flow control."""

from dataclasses import fields
import os
from typing import Any, Callable
from Engine import Filters, Color
from Global import Manager, playVideo, System as GlobalSystem
from Global.Weather import WeatherType

SoundFilter = Filters.SoundFilter()
MusicFilter = Filters.MusicFilter()


class _TransitionCondition:
    def __init__(self) -> None:
        self._started = False

    def __call__(self) -> bool:
        if GlobalSystem.isTransitionPending() or GlobalSystem.isInTransition():
            self._started = True
            return False
        return self._started


def _isTransitionBackgroundFrozen() -> bool:
    return (
        not GlobalSystem.isTransitionBackgroundFreezePending()
        and GlobalSystem.isTransitionBackgroundFrozen()
    )


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


@Meta(
    DisplayName='LOC("FREEZE_TRANSITION_BACKGROUND")',
    DisplayDesc='LOC("FREEZE_TRANSITION_BACKGROUND_DESC")',
)
@Latent(Frozen=(True,))
def FreezeTransitionBackground() -> Callable[[], bool]:
    r"""\brief Freeze the current frame and wait until it is ready for a transition.

    - \return A condition callable that becomes True when the frame has been captured.
    """
    GlobalSystem.freezeTransitionBackground()
    return _isTransitionBackgroundFrozen


@Meta(DisplayName='LOC("REQUEST_TRANSITION")', DisplayDesc='LOC("REQUEST_TRANSITION_DESC")')
@Latent(Finished=(True,))
def RequestTransition(transitionName: str, transitionTime: float) -> Callable[[], bool]:
    r"""\brief Request a screen transition and wait until it finishes.

    - \param transitionName Optional transition texture filename.
    - \param transitionTime Transition duration in seconds.
    - \return A condition callable that becomes True when the transition is finished.
    """
    GlobalSystem.requestTransition(transitionName, float(transitionTime))
    return _TransitionCondition()


@Meta(
    DisplayName='LOC("SET_BGM_FILTER")',
    DisplayDesc='LOC("SET_BGM_FILTER_DESC")',
    DropBox={"attr": [field.name for field in fields(Filters.MusicFilter)]},
)
@ExecSplit(default=(None,))
def SetBgmFilter(attr: str, value: Any) -> None:
    scene = GlobalSystem.getScene()
    if scene and hasattr(scene, "setBgmFilter"):
        scene.setBgmFilter(attr, value)


@Meta(
    DisplayName='LOC("SET_BGS_FILTER")',
    DisplayDesc='LOC("SET_BGS_FILTER_DESC")',
    DropBox={"attr": [field.name for field in fields(Filters.SoundFilter)]},
)
@ExecSplit(default=(None,))
def SetBgsFilter(attr: str, value: Any) -> None:
    scene = GlobalSystem.getScene()
    if scene and hasattr(scene, "setBgsFilter"):
        scene.setBgsFilter(attr, value)


@Meta(DisplayName='LOC("FLASH_SCREEN")', DisplayDesc='LOC("FLASH_SCREEN_DESC")')
@ExecSplit(default=(None,))
def FlashScreen(red: int, green: int, blue: int, alpha: int, duration: float) -> None:
    GlobalSystem.flashScreen(Color(int(red), int(green), int(blue), int(alpha)), float(duration))


@Meta(DisplayName='LOC("STOP_FLASH_SCREEN")', DisplayDesc='LOC("STOP_FLASH_SCREEN_DESC")')
@ExecSplit(default=(None,))
def StopFlashScreen() -> None:
    GlobalSystem.stopFlash()


@Meta(DisplayName='LOC("SCREEN_SHAKE")', DisplayDesc='LOC("SCREEN_SHAKE_DESC")')
@ExecSplit(default=(None,))
def ScreenShake(power: float, speed: float, duration: float) -> None:
    GlobalSystem.startShake(float(power), float(speed), float(duration))


@Meta(DisplayName='LOC("STOP_SCREEN_SHAKE")', DisplayDesc='LOC("STOP_SCREEN_SHAKE_DESC")')
@ExecSplit(default=(None,))
def StopScreenShake() -> None:
    GlobalSystem.stopShake()


@Meta(
    DisplayName='LOC("SET_WEATHER")',
    DisplayDesc='LOC("SET_WEATHER_DESC")',
    DropBox={"weatherType": WeatherType.dropBoxItems()},
)
@ExecSplit(default=(None,))
def SetWeather(weatherType: WeatherType, power: int, maxCount: int) -> None:
    GlobalSystem.setWeather(WeatherType.coerce(weatherType), int(power), int(maxCount))


@Meta(DisplayName='LOC("CLEAR_WEATHER")', DisplayDesc='LOC("CLEAR_WEATHER_DESC")')
@ExecSplit(default=(None,))
def ClearWeather() -> None:
    GlobalSystem.clearWeather()
