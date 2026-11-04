# Official Plug-ins Overview

Ludork includes five official C# plug-ins: Official Locale Tools, Official Random Map, Official Mota Map Fill, Official Blueprint AI and Official Resource Cleanup. Their source is also available as examples of the public plug-in API.

## The official plug-ins

| Plug-in | What it does |
|---|---|
| [Official Locale Tools](<Locale Tools.md>) | Opens the translation workbook, supplies text hints and exports game language catalogues. |
| [Official Random Map](<Random Map.md>) | Generates map regions and saves one layer with revision protection. |
| [Official Mota Map Fill](<Mota Map Fill.md>) | Fills existing map regions with doors, enemies and rewards using a saved player's battle damage. |
| [Official Blueprint AI](<Blueprint AI.md>) | Discusses one existing Blueprint and applies a validated, reviewable proposal as one Undo step. |
| [Official Resource Cleanup](<Resource Cleanup.md>) | Finds unreferenced resource candidates, protects keep-listed paths and recycles confirmed files. |

[Game Localisation Workflow](<Game Localisation Workflow.md>) covers workbook authoring, Export and runtime `{ID}` resolution with Official Locale Tools.

## Installation and management

See [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) for installation commands, skip options, restart requirements, status and retained data. Installing the official plug-ins replaces all existing global and project plug-ins, their registrations and their data on every platform. Close Ludork and back up anything needed before continuing.

## Related pages

- [Registration and Hook Reference](<../Registration and Hook Reference.md>) — the registration methods and hook contracts behind these plug-ins
