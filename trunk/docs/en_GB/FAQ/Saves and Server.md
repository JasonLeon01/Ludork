# Saves and Server

## Why do changed game-variable defaults not affect an existing save?

Saved variable declarations provide initial values for new games. Loading a save slot replaces the new instance's variable table with the saved table without merging newly added declarations or migrating existing values. Apply default changes in a new game or implement custom save migration. See [Game variables](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#game-variables>).

## Do I need Ludork Server, and does it provide multiplayer or cloud saves?

Ludork Server is an optional Node.js service with server Lua logic, explicit snapshot proxies, void RPCs and online events. Gameplay code decides when to fetch or refresh data and which business actions to send; the service neither synchronises multiplayer state automatically nor uploads, downloads or replaces local saves. Configuring client integration requires a **C++ Source** project. See [Simple Server](<../Simple Server/Deployment and Editor Settings.md#simple-server-deployment-and-editor-settings>) and [Configuration](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#configuration>).
