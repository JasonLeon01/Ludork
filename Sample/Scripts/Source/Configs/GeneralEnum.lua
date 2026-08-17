
local GeneralDataKey = {
    Class = "Class",
    Enemy = "Enemy",
    Equip = "Equip",
    Item = "Item",
    Player = "Player",
    Special = "Special",
    State = "State"
}

local ClassKeys = { Warrior = "Warrior" }

local Enemy = {
    Bat = "Bat",
    BigWizard = "BigWizard",
    GreatIron = "GreatIron",
    Knight = "Knight",
    Mage = "Mage",
    Rock = "Rock",
    Skeleton = "Skeleton",
    Slime = "Slime",
    WhiteKing = "WhiteKing",
    Wizard = "Wizard"
}

local Equip = { Shield_A = "Shield_A", Sword_A = "Sword_A" }

local Item = {
    BreakIce = "BreakIce",
    BreakLava = "BreakLava",
    BreakWall = "BreakWall",
    CentreFly = "CentreFly",
    ClearWall = "ClearWall",
    DownFly = "DownFly",
    EnemyBook = "EnemyBook",
    KEY_B = "KEY_B",
    KEY_R = "KEY_R",
    KEY_Y = "KEY_Y",
    PoisonedEase = "PoisonedEase",
    PoisonedRelease = "PoisonedRelease",
    Teleport = "Teleport",
    UpFly = "UpFly",
    WeakEase = "WeakEase",
    WeakRelease = "WeakRelease"
}

local Player = { Bravor = "Bravor" }

local Special = {
    Blockade = "Blockade",
    Compete = "Compete",
    Domain = "Domain",
    Flank = "Flank",
    Hard = "Hard",
    Magic = "Magic",
    MultiHit = "MultiHit",
    Poisoning = "Poisoning",
    Weaken = "Weaken"
}

local State = { Poisoned = "Poisoned", Weak = "Weak" }

return {
    GeneralDataKey = GeneralDataKey,
    Class = ClassKeys,
    Enemy = Enemy,
    Equip = Equip,
    Item = Item,
    Player = Player,
    Special = Special,
    State = State
}
