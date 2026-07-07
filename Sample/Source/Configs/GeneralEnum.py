# -*- encoding: utf-8 -*-

from __future__ import annotations

r"""
\brief Auto-generated General Data key constants.
"""


class GeneralDataKey:
    r"""\brief General Data table keys."""
    Class: str = "Class"
    Enemy: str = "Enemy"
    Equip: str = "Equip"
    Item: str = "Item"
    Player: str = "Player"
    Special: str = "Special"
    State: str = "State"


class Class:
    r"""\brief Class member keys."""
    Warrior: str = "Warrior"


class Enemy:
    r"""\brief Enemy member keys."""
    Bat: str = "Bat"
    BigWizard: str = "BigWizard"
    Knight: str = "Knight"
    Mage: str = "Mage"
    Rock: str = "Rock"
    Skeleton: str = "Skeleton"
    Slime: str = "Slime"
    WhiteKing: str = "WhiteKing"
    Wizard: str = "Wizard"


class Equip:
    r"""\brief Equip member keys."""
    Shield_A: str = "Shield_A"
    Sword_A: str = "Sword_A"


class Item:
    r"""\brief Item member keys."""
    BreakIce: str = "BreakIce"
    BreakLava: str = "BreakLava"
    BreakWall: str = "BreakWall"
    ClearWall: str = "ClearWall"
    EnemyBook: str = "EnemyBook"
    KEY_B: str = "KEY_B"
    KEY_R: str = "KEY_R"
    KEY_Y: str = "KEY_Y"
    PoisonedEase: str = "PoisonedEase"
    PoisonedRelease: str = "PoisonedRelease"
    Teleport: str = "Teleport"
    WeakEase: str = "WeakEase"
    WeakRelease: str = "WeakRelease"


class Player:
    r"""\brief Player member keys."""
    Bravor: str = "Bravor"


class Special:
    r"""\brief Special member keys."""
    Blockade: str = "Blockade"
    Compete: str = "Compete"
    Domain: str = "Domain"
    Flank: str = "Flank"
    Hard: str = "Hard"
    Magic: str = "Magic"
    MultiHit: str = "MultiHit"
    Poisoning: str = "Poisoning"
    Weaken: str = "Weaken"


class State:
    r"""\brief State member keys."""
    Poisoned: str = "Poisoned"
    Weak: str = "Weak"


__all__ = [
    "GeneralDataKey",
    "Class",
    "Enemy",
    "Equip",
    "Item",
    "Player",
    "Special",
    "State",
]
